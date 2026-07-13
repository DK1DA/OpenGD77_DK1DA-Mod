/*
 * Copyright (C) 2026-today      Marcus Kida, DK1DA
 *
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. Use of this source code or binary releases for commercial purposes is strictly forbidden. This includes, without limitation,
 *    incorporation in a commercial product or incorporation into a product or project which allows commercial use.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <string.h>
#include "functions/smsCore.h"
#include "functions/smsStorage.h"
#include "functions/ticks.h"
#include "functions/trx.h"
#include "functions/settings.h"
#include "hardware/HR-C6000.h"

#define SMS_WAIT_CHANNEL_TIMEOUT_MS     4000
#define SMS_WAIT_ACK_TIMEOUT_MS         6000
#define SMS_ACK_RESPONSE_DELAY_MS       1500
#define SMS_RX_ASSEMBLY_STALL_MS        2000
#define SMS_RX_DEDUPE_WINDOW_MS         30000

#define SMS_EVENT_QUEUE_SIZE            8

//
// Outgoing transmission
//
static struct
{
	volatile smsTxState_t state;
	volatile bool jobPending;
	bool jobIsAckResponse;
	uint32_t peerId;
	char text[SMS_MAX_TEXT_LENGTH + 1]; // kept for the sent box and resend
	bool haveLastMessage;
	ticksTimer_t channelTimer;
	ticksTimer_t ackTimer;
	uint16_t ipSequence;
} tx;

static smsAirJob_t txAirJob;

// set from ISR context, consumed in smsCoreTick()
static volatile bool txAirDoneFlag = false;
static volatile bool txRejectedFlag = false;
static volatile bool txDeliveredFlag = false;

//
// Incoming assembly (written from ISR context only).
//
// The raw bursts are kept as-received: senders differ in whether the blocks
// carry a confirmed-data prefix, and it is not guaranteed how many of a
// rate-3/4 block's 18 bytes the chip exposes, so the extraction layout is
// determined at decode time by trying the possible models against the IP
// header checksum.
//
#define SMS_RX_MAX_BLOCKS       32
#define SMS_RX_BLOCK_STRIDE     DMR_DATA_RATE34_BLOCK_LENGTH

static volatile struct
{
	bool active;
	uint8_t expectedBlocks;
	uint8_t receivedBlocks;
	uint8_t framesSeen; // progress marker for the stall watchdog
	uint32_t srcId;
	bool responseRequested;
	bool confirmedData;  // header DPF was "confirmed"
	bool rate34;         // payload arrived as rate-3/4 blocks
} rxAssembly;

// Shared between assembly (ISR) and decoding (main task): while
// rxDecode.pending is set the buffers belong to the main task and the ISR
// won't open a new assembly into them.
static uint8_t rxRawBlocks[SMS_RX_MAX_BLOCKS * SMS_RX_BLOCK_STRIDE];
static __attribute__((section(".ccmram"))) uint8_t rxAssembledPayload[SMS_RX_MAX_BLOCKS * (SMS_RX_BLOCK_STRIDE - DMR_DATA_CONFIRMED_PREFIX_LENGTH)];

// completed assembly handed over for decoding in the main task
static volatile struct
{
	bool pending;
	uint8_t blockCount;
	bool confirmedData;
	bool rate34;
	uint32_t srcId;
	bool responseRequested;
} rxDecode;

//
// Delivery report we owe to a sender
//
static struct
{
	bool pending;
	uint32_t dstId;
	ticksTimer_t delayTimer;
} ackResponse;

// UI event queue and unseen-message flag (main task context only)
static uint8_t eventQueue[SMS_EVENT_QUEUE_SIZE];
static uint8_t eventQueueHead = 0;
static uint8_t eventQueueCount = 0;

static bool unseenRxMessage = false;
static uint32_t lastRxStoreTime = 0;

static void pushEvent(smsEvent_t event)
{
	if (eventQueueCount < SMS_EVENT_QUEUE_SIZE)
	{
		eventQueue[(eventQueueHead + eventQueueCount) % SMS_EVENT_QUEUE_SIZE] = (uint8_t)event;
		eventQueueCount++;
	}
}

smsEvent_t smsPopEvent(void)
{
	if (eventQueueCount == 0)
	{
		return SMS_EVENT_NONE;
	}

	smsEvent_t event = (smsEvent_t)eventQueue[eventQueueHead];
	eventQueueHead = (eventQueueHead + 1) % SMS_EVENT_QUEUE_SIZE;
	eventQueueCount--;

	return event;
}

void smsCoreInit(void)
{
	memset(&tx, 0, sizeof(tx));
	memset((void *)&rxAssembly, 0, sizeof(rxAssembly));
	memset((void *)&rxDecode, 0, sizeof(rxDecode));
	memset(&ackResponse, 0, sizeof(ackResponse));
	eventQueueHead = 0;
	eventQueueCount = 0;
	unseenRxMessage = false;
	txAirDoneFlag = false;
	txRejectedFlag = false;
	txDeliveredFlag = false;
}

bool smsCoreIsIdle(void)
{
	return ((tx.state == SMS_TX_STATE_IDLE) && (tx.jobPending == false) &&
			(rxAssembly.active == false) && (rxDecode.pending == false) &&
			(ackResponse.pending == false));
}

smsTxState_t smsGetTxState(void)
{
	return tx.state;
}

//
// Air job construction. The payload blocks are built in place: the rows of
// txAirJob.frames are contiguous, so the encoder writes the whole payload
// directly into the rows following the preambles and the header.
//
static void buildAirJobFrames(uint32_t dstId, uint32_t srcId, uint8_t preambleCount,
		const uint8_t *headerFrame, uint8_t blockCount)
{
	uint8_t frameIndex = 0;

	for (uint8_t i = 0; i < preambleCount; i++)
	{
		// remaining frames after this preamble: the other preambles, the
		// header, and the payload blocks
		uint8_t blocksToFollow = (preambleCount - 1 - i) + 1 + blockCount;

		dmrDataBuildPreambleCSBK(txAirJob.frames[frameIndex], dstId, srcId, blocksToFollow);
		frameIndex++;
	}

	memcpy(txAirJob.frames[frameIndex], headerFrame, DMR_DATA_BURST_LENGTH);
	frameIndex++;

	// the payload block contents are already in place (message jobs), or absent (ACK jobs)
	txAirJob.preambleCount = preambleCount;
	txAirJob.frameCount = frameIndex + blockCount;
}

static bool queueMessageTransmission(uint32_t dstId, const char *text)
{
	uint16_t payloadLength = 0;
	uint8_t padOctets = 0;
	uint8_t *payloadArea = txAirJob.frames[SMS_TX_PREAMBLE_COUNT + 1]; // rows after preambles + header

	tx.ipSequence++;

	if (smsPayloadBuildMotorola(payloadArea, &payloadLength, &padOctets,
			dstId, trxDMRID, text, tx.ipSequence) != SMS_ENCODE_OK)
	{
		return false;
	}

	uint8_t blockCount = payloadLength / DMR_DATA_BURST_LENGTH;
	uint8_t headerFrame[DMR_DATA_BURST_LENGTH];

	dmrDataBuildUnconfirmedHeader(headerFrame, dstId, trxDMRID, blockCount, padOctets, smsOptionWaitForAck());
	buildAirJobFrames(dstId, trxDMRID, SMS_TX_PREAMBLE_COUNT, headerFrame, blockCount);

	tx.peerId = dstId;
	strncpy(tx.text, text, SMS_MAX_TEXT_LENGTH);
	tx.text[SMS_MAX_TEXT_LENGTH] = 0;
	tx.haveLastMessage = true;
	tx.jobIsAckResponse = false;
	tx.jobPending = true;
	tx.state = SMS_TX_STATE_WAIT_CHANNEL;
	ticksTimerStart(&tx.channelTimer, SMS_WAIT_CHANNEL_TIMEOUT_MS);

	return true;
}

static void queueAckResponseTransmission(uint32_t dstId)
{
	uint8_t headerFrame[DMR_DATA_BURST_LENGTH];
	// repeaters/hotspots expect the SAP + response-type variant of the
	// delivery report, OEM radios on simplex the plain one
	bool repeaterProfile = (currentChannelData->rxFreq != currentChannelData->txFreq);

	dmrDataBuildResponseHeader(headerFrame, dstId, trxDMRID, repeaterProfile);
	buildAirJobFrames(dstId, trxDMRID, SMS_TX_ACK_PREAMBLE_COUNT, headerFrame, 0);

	tx.jobIsAckResponse = true;
	tx.jobPending = true;
	tx.state = SMS_TX_STATE_WAIT_CHANNEL;
	ticksTimerStart(&tx.channelTimer, SMS_WAIT_CHANNEL_TIMEOUT_MS);
}

static bool smsTransmissionIsAllowed(void)
{
	return ((trxGetMode() == RADIO_MODE_DIGITAL) &&
			(settingsUsbMode != USB_MODE_HOTSPOT) &&
			(settingsIsOptionBitSet(BIT_TX_INHIBIT) == false) &&
			(trxTransmissionEnabled == false) &&
			(trxIsTransmitting == false));
}

static bool tryStartAirTransmission(void)
{
	return HRC6000StartSmsTransmission();
}

bool smsSendMessage(uint32_t dstId, const char *text)
{
	if ((tx.state != SMS_TX_STATE_IDLE) || tx.jobPending ||
			(dstId == 0) || (dstId > 0xFFFFFF) || (smsTransmissionIsAllowed() == false))
	{
		return false;
	}

	return queueMessageTransmission(dstId, text);
}

bool smsCanResendLast(void)
{
	return (tx.haveLastMessage && (tx.state == SMS_TX_STATE_IDLE) && (tx.jobPending == false));
}

bool smsResendLast(void)
{
	if (smsCanResendLast() == false)
	{
		return false;
	}

	return smsSendMessage(tx.peerId, tx.text);
}

//
// RX notification for the popup screen
//
bool smsHasUnseenRxMessage(void)
{
	return unseenRxMessage;
}

void smsAcknowledgeRxMessage(void)
{
	unseenRxMessage = false;
}

//
// Options (persisted with the message store)
//
bool smsOptionWaitForAck(void)
{
	return ((smsStorageGetOptions() & SMS_OPTION_WAIT_FOR_ACK) != 0);
}

void smsOptionSetWaitForAck(bool waitForAck)
{
	uint8_t options = smsStorageGetOptions();

	smsStorageSetOptions(waitForAck ? (options | SMS_OPTION_WAIT_FOR_ACK) : (options & ~SMS_OPTION_WAIT_FOR_ACK));
}

bool smsOptionOwnIdOnly(void)
{
	return ((smsStorageGetOptions() & SMS_OPTION_OWN_ID_ONLY) != 0);
}

void smsOptionSetOwnIdOnly(bool ownIdOnly)
{
	uint8_t options = smsStorageGetOptions();

	smsStorageSetOptions(ownIdOnly ? (options | SMS_OPTION_OWN_ID_ONLY) : (options & ~SMS_OPTION_OWN_ID_ONLY));
}

//
// ISR-side interface
//
const smsAirJob_t *smsGetPendingAirJob(void)
{
	return (tx.jobPending ? &txAirJob : NULL);
}

void smsAirJobStarted(void)
{
	tx.jobPending = false;
}

void smsTxNotifyAirDone(void)
{
	txAirDoneFlag = true;
}

void smsTxNotifyRejected(void)
{
	txRejectedFlag = true;
}

void smsTxNotifyHwAck(void)
{
	if (tx.state == SMS_TX_STATE_WAIT_ACK)
	{
		txDeliveredFlag = true;
	}
}

static void rxAssemblyReset(void)
{
	rxAssembly.active = false;
	rxAssembly.expectedBlocks = 0;
	rxAssembly.receivedBlocks = 0;
}

static void rxAssemblyComplete(void)
{
	rxDecode.blockCount = ((rxAssembly.receivedBlocks < SMS_RX_MAX_BLOCKS) ? rxAssembly.receivedBlocks : SMS_RX_MAX_BLOCKS);
	rxDecode.confirmedData = rxAssembly.confirmedData;
	rxDecode.rate34 = rxAssembly.rate34;
	rxDecode.srcId = rxAssembly.srcId;
	rxDecode.responseRequested = rxAssembly.responseRequested;
	rxDecode.pending = true; // buffers now belong to the main task

	rxAssemblyReset();
}

bool smsRxProcessDataFrame(uint8_t dataType, const uint8_t *frame)
{
	rxAssembly.framesSeen++;

	if (dataType == DMR_DATA_TYPE_DATA_HEADER)
	{
		dmrDataHeader_t header;

		if (dmrDataParseHeader(frame, &header) == false)
		{
			return false;
		}

		// delivery report for our outstanding message? (it can arrive while
		// the state machine is still finishing up the transmission)
		if (header.isResponsePdu)
		{
			if (((tx.state == SMS_TX_STATE_WAIT_ACK) || (tx.state == SMS_TX_STATE_SENDING)) &&
					(header.srcId == tx.peerId))
			{
				txDeliveredFlag = true;
			}

			return true;
		}

		// Longer messages than the raw-block store still assemble; the text
		// beyond the stored blocks is truncated at decode time
		if ((header.sap != 0x04) || header.isGroup ||
				(header.blockCount == 0) || (header.blockCount > SMS_MAX_DATA_BLOCKS))
		{
			return false;
		}

		if (smsOptionOwnIdOnly() && (header.dstId != trxDMRID))
		{
			return false;
		}

		if (rxDecode.pending) // buffers still owned by the decoder, drop this message
		{
			return false;
		}

		rxAssembly.expectedBlocks = header.blockCount;
		rxAssembly.receivedBlocks = 0;
		rxAssembly.srcId = header.srcId;
		rxAssembly.responseRequested = header.responseRequested;
		rxAssembly.confirmedData = (header.dpf == 0x03);
		rxAssembly.rate34 = false;
		rxAssembly.active = true;

		return true;
	}

	if (rxAssembly.active == false)
	{
		return false;
	}

	if ((dataType != DMR_DATA_TYPE_RATE_12_DATA) && (dataType != DMR_DATA_TYPE_RATE_34_DATA))
	{
		return false;
	}

	if (rxAssembly.receivedBlocks < SMS_RX_MAX_BLOCKS)
	{
		// keep the burst raw; the extraction layout is decided at decode time
		memcpy(&rxRawBlocks[rxAssembly.receivedBlocks * SMS_RX_BLOCK_STRIDE], frame,
				((dataType == DMR_DATA_TYPE_RATE_34_DATA) ? DMR_DATA_RATE34_BLOCK_LENGTH : DMR_DATA_BURST_LENGTH));

		if (rxAssembly.receivedBlocks == 0)
		{
			rxAssembly.rate34 = (dataType == DMR_DATA_TYPE_RATE_34_DATA);
		}
	}

	rxAssembly.receivedBlocks++;

	if (rxAssembly.receivedBlocks >= rxAssembly.expectedBlocks)
	{
		rxAssemblyComplete();
	}

	return true;
}

//
// Main task processing
//

// Compact the raw bursts into a contiguous payload, taking `take` bytes
// starting at `skip` from each block. Returns the assembled length.
static uint16_t rxExtractPayload(uint8_t blockCount, uint8_t skip, uint8_t take)
{
	for (uint8_t i = 0; i < blockCount; i++)
	{
		memcpy(&rxAssembledPayload[i * take], &rxRawBlocks[(i * SMS_RX_BLOCK_STRIDE) + skip], take);
	}

	return (uint16_t)blockCount * take;
}

static void processCompletedRxMessage(void)
{
	char text[SMS_MAX_TEXT_LENGTH + 1];
	uint32_t srcId = rxDecode.srcId;
	bool responseRequested = rxDecode.responseRequested;

	// Candidate block layouts, likeliest first (based on the header's
	// confirmed/unconfirmed DPF). For rate-3/4, the 12-byte layouts cover
	// the case where the chip exposes only part of the 18-byte block.
	const uint8_t prefix = DMR_DATA_CONFIRMED_PREFIX_LENGTH;
	uint8_t blockLengths[4];
	uint8_t skips[4];
	uint8_t modelCount;

	if (rxDecode.rate34)
	{
		blockLengths[0] = DMR_DATA_RATE34_BLOCK_LENGTH;
		blockLengths[1] = DMR_DATA_BURST_LENGTH;
		blockLengths[2] = DMR_DATA_RATE34_BLOCK_LENGTH;
		blockLengths[3] = DMR_DATA_BURST_LENGTH;
		skips[0] = skips[1] = (rxDecode.confirmedData ? prefix : 0);
		skips[2] = skips[3] = (rxDecode.confirmedData ? 0 : prefix);
		modelCount = 4;
	}
	else
	{
		blockLengths[0] = blockLengths[1] = DMR_DATA_BURST_LENGTH;
		skips[0] = (rxDecode.confirmedData ? prefix : 0);
		skips[1] = (rxDecode.confirmedData ? 0 : prefix);
		modelCount = 2;
	}

	bool decoded = false;
	uint16_t length = 0;

	for (uint8_t m = 0; (m < modelCount) && (decoded == false); m++)
	{
		length = rxExtractPayload(rxDecode.blockCount, skips[m], blockLengths[m] - skips[m]);

		decoded = (smsPayloadParseMotorola(rxAssembledPayload, length, text, sizeof(text)) ||
				smsPayloadParseStandard(rxAssembledPayload, length, text, sizeof(text)));
	}

	if (decoded == false)
	{
		// No layout produced a structurally valid packet: salvage readable
		// text from every layout and keep the longest result
		char candidate[SMS_MAX_TEXT_LENGTH + 1];

		text[0] = 0;

		for (uint8_t m = 0; m < modelCount; m++)
		{
			length = rxExtractPayload(rxDecode.blockCount, skips[m], blockLengths[m] - skips[m]);

			if (smsPayloadScanText(rxAssembledPayload, length, candidate, sizeof(candidate)) &&
					(strlen(candidate) > strlen(text)))
			{
				memcpy(text, candidate, sizeof(text));
			}
		}

		decoded = (text[0] != 0);
	}

	rxDecode.pending = false; // buffers released back to the ISR assembler

	if (decoded)
	{
		// OEM radios repeat a message when they miss our delivery report;
		// don't store consecutive identical copies arriving close together
		const smsRecord_t *newest = smsInboxGet(0);
		bool isDuplicate = ((newest != NULL) && (newest->peerId == srcId) &&
				(strcmp(newest->text, text) == 0) &&
				((ticksGetMillis() - lastRxStoreTime) < SMS_RX_DEDUPE_WINDOW_MS));

		if (isDuplicate == false)
		{
			smsInboxAdd(srcId, text);
			lastRxStoreTime = ticksGetMillis();
			unseenRxMessage = true;
			pushEvent(SMS_EVENT_RX_MESSAGE);
		}
	}

	// acknowledge receipt at the data-link level even when the payload
	// didn't decode to text - all blocks did arrive
	if (responseRequested && (srcId != 0) && (srcId != trxDMRID))
	{
		ackResponse.pending = true;
		ackResponse.dstId = srcId;
		ticksTimerStart(&ackResponse.delayTimer, SMS_ACK_RESPONSE_DELAY_MS);
	}
}

static void watchRxAssemblyStall(void)
{
	static uint8_t lastFramesSeen = 0;
	static ticksTimer_t stallTimer = { 0, 0 };
	static bool watching = false;

	if (rxAssembly.active)
	{
		if ((watching == false) || (rxAssembly.framesSeen != lastFramesSeen))
		{
			lastFramesSeen = rxAssembly.framesSeen;
			ticksTimerStart(&stallTimer, SMS_RX_ASSEMBLY_STALL_MS);
			watching = true;
		}
		else if (ticksTimerHasExpired(&stallTimer))
		{
			rxAssemblyReset(); // transmission broke off mid-message
			watching = false;
		}
	}
	else
	{
		watching = false;
	}
}

static void finishTransmission(smsEvent_t event)
{
	bool wasAckResponse = tx.jobIsAckResponse;

	tx.state = SMS_TX_STATE_IDLE;
	tx.jobPending = false;
	tx.jobIsAckResponse = false;

	if ((wasAckResponse == false) && (event != SMS_EVENT_NONE))
	{
		pushEvent(event);
	}
}

void smsCoreTick(void)
{
	if (rxDecode.pending)
	{
		processCompletedRxMessage();
	}

	watchRxAssemblyStall();

	// pending delivery report: transmit once the delay elapsed and we're free
	if (ackResponse.pending && ticksTimerHasExpired(&ackResponse.delayTimer) &&
			(tx.state == SMS_TX_STATE_IDLE) && (tx.jobPending == false))
	{
		if (smsTransmissionIsAllowed())
		{
			queueAckResponseTransmission(ackResponse.dstId);
		}

		ackResponse.pending = false; // single attempt, dropped when not possible
	}

	switch (tx.state)
	{
		case SMS_TX_STATE_WAIT_CHANNEL:
			if (tryStartAirTransmission())
			{
				tx.state = SMS_TX_STATE_SENDING;

				if (tx.jobIsAckResponse == false)
				{
					pushEvent(SMS_EVENT_TX_SENDING);
				}
			}
			else if (ticksTimerHasExpired(&tx.channelTimer))
			{
				finishTransmission(SMS_EVENT_TX_CHANNEL_BUSY);
			}
			break;

		case SMS_TX_STATE_SENDING:
			if (HRC6000GetIsWakingState() == WAKING_MODE_FAILED)
			{
				// The repeater never woke up; tear the transmission down
				HRC6000CancelSmsTransmission();
				trxTransmissionEnabled = false;
				HRC6000ClearIsWakingState();
				HRC6000TerminateDigital();
				trxDisableTransmission();
				finishTransmission(SMS_EVENT_TX_CHANNEL_BUSY);
			}
			else if (txRejectedFlag)
			{
				txRejectedFlag = false;
				trxDisableTransmission();
				finishTransmission(SMS_EVENT_TX_CHANNEL_BUSY);
			}
			else if (txAirDoneFlag)
			{
				txAirDoneFlag = false;

				if (tx.jobIsAckResponse)
				{
					finishTransmission(SMS_EVENT_NONE);
				}
				else
				{
					smsSentAdd(tx.peerId, tx.text);
					pushEvent(SMS_EVENT_TX_SENT);

					if (smsOptionWaitForAck())
					{
						tx.state = SMS_TX_STATE_WAIT_ACK;
						ticksTimerStart(&tx.ackTimer, SMS_WAIT_ACK_TIMEOUT_MS);
					}
					else
					{
						finishTransmission(SMS_EVENT_NONE);
					}
				}
			}
			break;

		case SMS_TX_STATE_WAIT_ACK:
			if (txDeliveredFlag)
			{
				txDeliveredFlag = false;
				finishTransmission(SMS_EVENT_TX_DELIVERED);
			}
			else if (ticksTimerHasExpired(&tx.ackTimer))
			{
				finishTransmission(SMS_EVENT_TX_TIMEOUT);
			}
			break;

		case SMS_TX_STATE_IDLE:
		default:
			txAirDoneFlag = false;
			txRejectedFlag = false;
			txDeliveredFlag = false;
			break;
	}
}
