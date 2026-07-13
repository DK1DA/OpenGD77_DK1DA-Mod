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

/*
 * DMR SMS engine: outgoing message queue and delivery tracking, incoming
 * message assembly and decoding, and delivery-report (ACK) handling.
 *
 * Context rules:
 *  - smsRxProcessDataFrame() and the smsTxNotify*() functions are called from
 *    the HR-C6000 interrupt handlers only.
 *  - Everything else runs in the main application task. Decoding, storage
 *    and event delivery are deferred from ISR to smsCoreTick().
 */
#ifndef _OPENGD77_SMS_CORE_H_
#define _OPENGD77_SMS_CORE_H_

#include <stdint.h>
#include <stdbool.h>
#include "functions/dmrDataProtocol.h"

#define SMS_TX_PREAMBLE_COUNT           8
#define SMS_TX_ACK_PREAMBLE_COUNT       24
// large enough for a full message (8 preambles + header + 31 blocks) and
// for a delivery report (24 preambles + header, no blocks)
#define SMS_TX_MAX_FRAMES               (SMS_TX_PREAMBLE_COUNT + 1 + SMS_MAX_TX_DATA_BLOCKS)

typedef enum
{
	SMS_TX_STATE_IDLE = 0,
	SMS_TX_STATE_WAIT_CHANNEL,  // packed, waiting for the channel to become free
	SMS_TX_STATE_SENDING,       // bursts going out
	SMS_TX_STATE_WAIT_ACK       // sent, waiting for a delivery report
} smsTxState_t;

typedef enum
{
	SMS_EVENT_NONE = 0,
	SMS_EVENT_TX_SENDING,
	SMS_EVENT_TX_SENT,
	SMS_EVENT_TX_DELIVERED,
	SMS_EVENT_TX_TIMEOUT,       // no delivery report received
	SMS_EVENT_TX_CHANNEL_BUSY,
	SMS_EVENT_RX_MESSAGE
} smsEvent_t;

// One prepared over-the-air transmission: preambleCount CSBK frames, then
// the data header, then the payload blocks. The per-frame DMR data type
// follows from the frame index.
typedef struct
{
	uint8_t frameCount;
	uint8_t preambleCount;
	uint8_t frames[SMS_TX_MAX_FRAMES][DMR_DATA_BURST_LENGTH];
} smsAirJob_t;

//
// Main task context
//
void smsCoreInit(void);
void smsCoreTick(void);
bool smsCoreIsIdle(void);       // nothing in flight (used to gate flash writes)

bool smsSendMessage(uint32_t dstId, const char *text);
bool smsResendLast(void);
bool smsCanResendLast(void);
smsTxState_t smsGetTxState(void);

smsEvent_t smsPopEvent(void);

bool smsHasUnseenRxMessage(void);
void smsAcknowledgeRxMessage(void);

bool smsOptionWaitForAck(void);
void smsOptionSetWaitForAck(bool waitForAck);
bool smsOptionOwnIdOnly(void);
void smsOptionSetOwnIdOnly(bool ownIdOnly);

//
// HR-C6000 driver interface (ISR context)
//
// frame carries the burst's logical payload: 18 bytes for rate-3/4 blocks,
// 12 bytes for everything else.
bool smsRxProcessDataFrame(uint8_t dataType, const uint8_t *frame);
void smsTxNotifyAirDone(void);
void smsTxNotifyRejected(void);
void smsTxNotifyHwAck(void);
const smsAirJob_t *smsGetPendingAirJob(void);
void smsAirJobStarted(void);

#endif /* _OPENGD77_SMS_CORE_H_ */
