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
#include "functions/dmrDataProtocol.h"

// CRC masks from ETSI TS 102 361-1 B.3.11/B.3.12 (per data type)
#define CSBK_CRC_MASK                   0xA5A5
#define DATA_HEADER_CRC_MASK            0xCCCC

// Data Packet Format values (data header octet 0, low nibble)
#define DPF_RESPONSE                    0x01
#define DPF_UNCONFIRMED                 0x02
#define DPF_CONFIRMED                   0x03

#define SAP_UDP_IP                      0x04

#define IP_HEADER_LENGTH                20
#define UDP_HEADER_LENGTH               8
#define TMS_HEADER_LENGTH               10
#define STANDARD_HEADER_LENGTH          4
#define CRC32_TRAILER_LENGTH            4

// TMS message type octet (payload offset 30): 0xA0 plain text,
// 0xE0 text with delivery-report request.
#define TMS_TYPE_TEXT                   0xA0
#define TMS_TYPE_TEXT_ACK_REQUEST       0xE0


uint16_t dmrDataCRC16(const uint8_t *data, uint16_t length)
{
	uint16_t crc = 0x0000;

	for (uint16_t i = 0; i < length; i++)
	{
		crc ^= ((uint16_t)data[i]) << 8;

		for (uint8_t bit = 0; bit < 8; bit++)
		{
			crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
		}
	}

	return crc;
}

// ETSI DMR CRC32 (poly 0x04C11DB7, init 0, MSB first). The input is consumed
// as byte-swapped 16-bit pairs, hence length must be even.
uint32_t dmrDataCRC32(const uint8_t *data, uint16_t length)
{
	uint32_t crc = 0;

	for (uint16_t i = 0; i < length; i++)
	{
		crc ^= ((uint32_t)data[i ^ 1]) << 24;

		for (uint8_t bit = 0; bit < 8; bit++)
		{
			crc = (crc & 0x80000000U) ? ((crc << 1) ^ 0x04C11DB7U) : (crc << 1);
		}
	}

	return crc;
}

// Ones-complement sum used by both the IP header and UDP checksums
static uint32_t onesComplementSum(uint32_t sum, const uint8_t *data, uint16_t length)
{
	uint16_t i = 0;

	while (length > 1)
	{
		sum += (((uint16_t)data[i]) << 8) | data[i + 1];
		i += 2;
		length -= 2;
	}

	if (length == 1)
	{
		sum += ((uint16_t)data[i]) << 8;
	}

	return sum;
}

static uint16_t onesComplementFold(uint32_t sum)
{
	while (sum >> 16)
	{
		sum = (sum & 0xFFFF) + (sum >> 16);
	}

	return (uint16_t)(~sum);
}

uint16_t dmrDataIPHeaderChecksum(const uint8_t *ipHeader)
{
	return onesComplementFold(onesComplementSum(0, ipHeader, IP_HEADER_LENGTH));
}

uint16_t dmrDataUDPChecksum(const uint8_t *ipPacket, uint16_t udpLength)
{
	uint8_t pseudoHeader[12];

	memcpy(&pseudoHeader[0], &ipPacket[12], 4); // source IP
	memcpy(&pseudoHeader[4], &ipPacket[16], 4); // destination IP
	pseudoHeader[8] = 0x00;
	pseudoHeader[9] = 0x11; // UDP protocol
	pseudoHeader[10] = (udpLength >> 8) & 0xFF;
	pseudoHeader[11] = udpLength & 0xFF;

	uint32_t sum = onesComplementSum(0, pseudoHeader, sizeof(pseudoHeader));
	uint16_t checksum = onesComplementFold(onesComplementSum(sum, &ipPacket[IP_HEADER_LENGTH], udpLength));

	// All-zero means "no checksum" in UDP, transmit as 0xFFFF instead
	return ((checksum == 0x0000) ? 0xFFFF : checksum);
}

static void put24BE(uint8_t *dest, uint32_t value)
{
	dest[0] = (value >> 16) & 0xFF;
	dest[1] = (value >> 8) & 0xFF;
	dest[2] = value & 0xFF;
}

static uint32_t get24BE(const uint8_t *src)
{
	return (((uint32_t)src[0]) << 16) | (((uint32_t)src[1]) << 8) | src[2];
}

// ETSI TS 102 361-1 B.3.11: the CCITT CRC is ones-complemented, then XORed
// with the data-type mask (validated against on-air captures).
static void applyFrameCRC(uint8_t frame[DMR_DATA_BURST_LENGTH], uint16_t mask)
{
	uint16_t crc = (uint16_t)(~dmrDataCRC16(frame, DMR_DATA_BURST_LENGTH - 2)) ^ mask;

	frame[10] = (crc >> 8) & 0xFF;
	frame[11] = crc & 0xFF;
}

void dmrDataBuildPreambleCSBK(uint8_t frame[DMR_DATA_BURST_LENGTH], uint32_t dstId, uint32_t srcId, uint8_t blocksToFollow)
{
	frame[0] = 0xBD; // Last Block flag + Preamble CSBK opcode (0x3D)
	frame[1] = 0x00; // Feature set ID: standardized
	frame[2] = 0x80; // data content follows, target is an individual ID
	frame[3] = blocksToFollow;
	put24BE(&frame[4], dstId);
	put24BE(&frame[7], srcId);
	applyFrameCRC(frame, CSBK_CRC_MASK);
}

void dmrDataBuildUnconfirmedHeader(uint8_t frame[DMR_DATA_BURST_LENGTH], uint32_t dstId, uint32_t srcId,
		uint8_t blockCount, uint8_t padOctets, bool requestResponse)
{
	frame[0] = DPF_UNCONFIRMED | (requestResponse ? 0x40 : 0x00) | (padOctets & 0x10);
	frame[1] = (SAP_UDP_IP << 4) | (padOctets & 0x0F);
	put24BE(&frame[2], dstId);
	put24BE(&frame[5], srcId);
	frame[8] = 0x80 | (blockCount & 0x7F); // full message, blocks to follow
	frame[9] = 0x00; // fragment sequence number
	applyFrameCRC(frame, DATA_HEADER_CRC_MASK);
}

void dmrDataBuildResponseHeader(uint8_t frame[DMR_DATA_BURST_LENGTH], uint32_t dstId, uint32_t srcId, bool repeaterProfile)
{
	frame[0] = DPF_RESPONSE;
	frame[1] = repeaterProfile ? (SAP_UDP_IP << 4) : 0x00;
	put24BE(&frame[2], dstId);
	put24BE(&frame[5], srcId);
	frame[8] = 0x00; // no blocks to follow
	frame[9] = repeaterProfile ? 0x08 : 0x00; // response type: ACK
	applyFrameCRC(frame, DATA_HEADER_CRC_MASK);
}

bool dmrDataParseHeader(const uint8_t frame[DMR_DATA_BURST_LENGTH], dmrDataHeader_t *header)
{
	uint16_t receivedCRC = (((uint16_t)frame[10]) << 8) | frame[11];

	if (((uint16_t)(~dmrDataCRC16(frame, DMR_DATA_BURST_LENGTH - 2)) ^ DATA_HEADER_CRC_MASK) != receivedCRC)
	{
		return false;
	}

	header->isGroup = ((frame[0] & 0x80) != 0);
	header->responseRequested = ((frame[0] & 0x40) != 0);
	header->dpf = frame[0] & 0x0F;
	header->sap = (frame[1] >> 4) & 0x0F;
	header->padOctets = (frame[0] & 0x10) | (frame[1] & 0x0F);
	header->dstId = get24BE(&frame[2]);
	header->srcId = get24BE(&frame[5]);
	header->blockCount = frame[8] & 0x7F;
	header->isResponsePdu = ((header->dpf == DPF_RESPONSE) && (frame[8] == 0x00));

	switch (header->dpf)
	{
		case DPF_RESPONSE:
		case DPF_UNCONFIRMED:
		case DPF_CONFIRMED:
			return true;

		default:
			return false;
	}
}

// Map one UTF-16 code unit to displayable ASCII. Both endiannesses appear in
// the wild (spec examples and BrandMeister use little-endian), so decode is
// per-character adaptive.
static char utf16ToChar(uint8_t first, uint8_t second)
{
	uint8_t ascii;

	if (second == 0x00)
	{
		ascii = first; // little-endian
	}
	else if (first == 0x00)
	{
		ascii = second; // big-endian
	}
	else
	{
		return '?'; // non latin-1 code point
	}

	if ((ascii >= 0x20) && (ascii < 0x7F))
	{
		return (char)ascii;
	}

	if ((ascii == '\r') || (ascii == '\n') || (ascii == '\t'))
	{
		return ' '; // messages are displayed as a single flowed text
	}

	return '?';
}

static void decodeUtf16Text(const uint8_t *data, uint16_t byteCount, char *textOut, uint16_t textOutSize)
{
	uint16_t outPos = 0;

	for (uint16_t i = 0; (i + 1) < byteCount; i += 2)
	{
		if (outPos >= (textOutSize - 1))
		{
			break;
		}

		if ((data[i] == 0x00) && (data[i + 1] == 0x00))
		{
			break; // embedded terminator
		}

		textOut[outPos++] = utf16ToChar(data[i], data[i + 1]);
	}

	// strip trailing '?' produced by stray padding
	while ((outPos > 0) && (textOut[outPos - 1] == '?'))
	{
		outPos--;
	}

	textOut[outPos] = 0;
}

smsEncodeResult_t smsPayloadBuildMotorola(uint8_t *payload, uint16_t *payloadLength, uint8_t *padOctets,
		uint32_t dstId, uint32_t srcId, const char *text, uint16_t ipSequenceNumber)
{
	uint16_t textLength = (uint16_t)strlen(text);

	if (textLength == 0)
	{
		return SMS_ENCODE_EMPTY;
	}

	if (textLength > SMS_MAX_TEXT_LENGTH)
	{
		return SMS_ENCODE_TOO_LONG;
	}

	uint16_t textBytes = textLength * 2;
	uint16_t ipLength = SMS_TEXT_OFFSET_MOTOROLA + textBytes; // headers + text, excludes padding and CRC32
	uint16_t udpLength = UDP_HEADER_LENGTH + TMS_HEADER_LENGTH + textBytes;
	uint16_t totalLength = ipLength + CRC32_TRAILER_LENGTH;

	// round up to a whole number of rate-1/2 blocks
	totalLength = ((totalLength + (DMR_DATA_BURST_LENGTH - 1)) / DMR_DATA_BURST_LENGTH) * DMR_DATA_BURST_LENGTH;

	uint8_t pad = (uint8_t)(totalLength - ipLength - CRC32_TRAILER_LENGTH);

	memset(payload, 0, totalLength);

	// IPv4 header
	payload[0] = 0x45; // version 4, IHL 5
	payload[1] = 0x00;
	payload[2] = (ipLength >> 8) & 0xFF;
	payload[3] = ipLength & 0xFF;
	payload[4] = (ipSequenceNumber >> 8) & 0xFF;
	payload[5] = ipSequenceNumber & 0xFF;
	payload[6] = 0x00; // flags / fragment offset
	payload[7] = 0x00;
	payload[8] = 0x01; // TTL
	payload[9] = 0x11; // UDP
	payload[10] = 0x00; // checksum placeholder
	payload[11] = 0x00;
	payload[12] = 0x0C; // individual DMR ID address space
	put24BE(&payload[13], srcId);
	payload[16] = 0x0C;
	put24BE(&payload[17], dstId);

	uint16_t ipChecksum = dmrDataIPHeaderChecksum(payload);
	payload[10] = (ipChecksum >> 8) & 0xFF;
	payload[11] = ipChecksum & 0xFF;

	// UDP header
	payload[20] = (SMS_UDP_PORT_MOTOROLA >> 8) & 0xFF;
	payload[21] = SMS_UDP_PORT_MOTOROLA & 0xFF;
	payload[22] = (SMS_UDP_PORT_MOTOROLA >> 8) & 0xFF;
	payload[23] = SMS_UDP_PORT_MOTOROLA & 0xFF;
	payload[24] = (udpLength >> 8) & 0xFF;
	payload[25] = udpLength & 0xFF;
	payload[26] = 0x00; // checksum placeholder
	payload[27] = 0x00;

	// TMS header
	payload[28] = 0x00;
	payload[29] = (textBytes + 8) & 0xFF;
	payload[30] = TMS_TYPE_TEXT_ACK_REQUEST;
	payload[31] = 0x00;
	payload[32] = (ipSequenceNumber & 0x7F) | 0x80;
	payload[33] = 0x04;
	payload[34] = 0x0D;
	payload[35] = 0x00;
	payload[36] = 0x0A;
	payload[37] = 0x00;

	// UTF-16LE text, uppercased ASCII for best cross-vendor display compatibility
	for (uint16_t i = 0; i < textLength; i++)
	{
		char c = text[i];

		if ((c >= 'a') && (c <= 'z'))
		{
			c -= ('a' - 'A');
		}
		else if (((c < 0x20) || (c >= 0x7F)) && (c != '\r') && (c != '\n'))
		{
			c = ' ';
		}

		payload[SMS_TEXT_OFFSET_MOTOROLA + (i * 2)] = (uint8_t)c;
		payload[SMS_TEXT_OFFSET_MOTOROLA + (i * 2) + 1] = 0x00;
	}

	uint16_t udpChecksum = dmrDataUDPChecksum(payload, udpLength);
	payload[26] = (udpChecksum >> 8) & 0xFF;
	payload[27] = udpChecksum & 0xFF;

	// trailing CRC32 (little-endian) over everything before it
	uint32_t crc = dmrDataCRC32(payload, totalLength - CRC32_TRAILER_LENGTH);
	payload[totalLength - 4] = crc & 0xFF;
	payload[totalLength - 3] = (crc >> 8) & 0xFF;
	payload[totalLength - 2] = (crc >> 16) & 0xFF;
	payload[totalLength - 1] = (crc >> 24) & 0xFF;

	*payloadLength = totalLength;
	*padOctets = pad;

	return SMS_ENCODE_OK;
}

// Common IP/UDP shell validation for both text formats. Returns the UDP
// length, or 0 when the shell is not a plausible SMS packet.
static uint16_t validateIPShell(const uint8_t *payload, uint16_t payloadLength, uint16_t udpPort)
{
	if (payloadLength < (IP_HEADER_LENGTH + UDP_HEADER_LENGTH))
	{
		return 0;
	}

	if (((payload[0] & 0xF0) != 0x40) || (payload[9] != 0x11)) // IPv4, UDP
	{
		return 0;
	}

	if (dmrDataIPHeaderChecksum(payload) != 0) // sum including the checksum field yields 0 when valid
	{
		return 0;
	}

	uint16_t srcPort = (((uint16_t)payload[20]) << 8) | payload[21];
	uint16_t dstPort = (((uint16_t)payload[22]) << 8) | payload[23];

	if ((srcPort != udpPort) && (dstPort != udpPort))
	{
		return 0;
	}

	uint16_t udpLength = (((uint16_t)payload[24]) << 8) | payload[25];

	if ((udpLength < UDP_HEADER_LENGTH) || ((IP_HEADER_LENGTH + udpLength) > payloadLength))
	{
		return 0;
	}

	return udpLength;
}

bool smsPayloadParseMotorola(const uint8_t *payload, uint16_t payloadLength, char *textOut, uint16_t textOutSize)
{
	uint16_t udpLength = validateIPShell(payload, payloadLength, SMS_UDP_PORT_MOTOROLA);

	if (udpLength < (UDP_HEADER_LENGTH + TMS_HEADER_LENGTH))
	{
		return false;
	}

	// TMS header shape: start marker, message type, "text" magic
	if ((payload[28] != 0x00) ||
			((payload[30] != TMS_TYPE_TEXT) && (payload[30] != TMS_TYPE_TEXT_ACK_REQUEST)) ||
			(payload[34] != 0x0D) || (payload[36] != 0x0A))
	{
		return false;
	}

	uint16_t textBytes = udpLength - UDP_HEADER_LENGTH - TMS_HEADER_LENGTH;

	if ((SMS_TEXT_OFFSET_MOTOROLA + textBytes) > payloadLength)
	{
		textBytes = payloadLength - SMS_TEXT_OFFSET_MOTOROLA;
	}

	decodeUtf16Text(&payload[SMS_TEXT_OFFSET_MOTOROLA], textBytes, textOut, textOutSize);

	return (textOut[0] != 0);
}

bool smsPayloadParseStandard(const uint8_t *payload, uint16_t payloadLength, char *textOut, uint16_t textOutSize)
{
	uint16_t udpLength = validateIPShell(payload, payloadLength, SMS_UDP_PORT_STANDARD);

	if (udpLength < (UDP_HEADER_LENGTH + STANDARD_HEADER_LENGTH))
	{
		return false;
	}

	if ((payload[28] != 0x00) || (payload[29] != 0x0D) || (payload[31] != 0x0A))
	{
		return false;
	}

	uint16_t textBytes = udpLength - UDP_HEADER_LENGTH - STANDARD_HEADER_LENGTH;

	if ((SMS_TEXT_OFFSET_STANDARD + textBytes) > payloadLength)
	{
		textBytes = payloadLength - SMS_TEXT_OFFSET_STANDARD;
	}

	decodeUtf16Text(&payload[SMS_TEXT_OFFSET_STANDARD], textBytes, textOut, textOutSize);

	return (textOut[0] != 0);
}

static bool isDisplayableAscii(uint8_t c)
{
	return (((c >= 0x20) && (c < 0x7F)) || (c == '\r') || (c == '\n'));
}

bool smsPayloadScanText(const uint8_t *payload, uint16_t payloadLength, char *textOut, uint16_t textOutSize)
{
	uint16_t bestStart = 0;
	uint16_t bestLength = 0; // in characters
	bool bestBigEndian = false;

	// four sweeps: both 16-bit alignments x both endiannesses
	for (uint8_t sweep = 0; sweep < 4; sweep++)
	{
		uint16_t start = sweep & 1;
		bool bigEndian = ((sweep & 2) != 0);
		uint16_t runStart = start;
		uint16_t runLength = 0;

		for (uint16_t i = start; (i + 1) < payloadLength; i += 2)
		{
			uint8_t asciiByte = bigEndian ? payload[i + 1] : payload[i];
			uint8_t zeroByte = bigEndian ? payload[i] : payload[i + 1];

			if ((zeroByte == 0x00) && isDisplayableAscii(asciiByte))
			{
				if (runLength == 0)
				{
					runStart = i;
				}

				runLength++;

				if (runLength > bestLength)
				{
					bestLength = runLength;
					bestStart = runStart;
					bestBigEndian = bigEndian;
				}
			}
			else
			{
				runLength = 0;
			}
		}
	}

	if (bestLength < 4)
	{
		return false;
	}

	uint16_t outPos = 0;

	for (uint16_t i = 0; (i < bestLength) && (outPos < (textOutSize - 1)); i++)
	{
		uint16_t offset = bestStart + (i * 2);
		char c = (char)(bestBigEndian ? payload[offset + 1] : payload[offset]);

		if ((c == '\r') || (c == '\n'))
		{
			c = ' '; // messages are displayed as a single flowed text
		}

		textOut[outPos++] = c;
	}

	textOut[outPos] = 0;

	// collapse the leading whitespace a run picked up from line breaks
	uint16_t start = 0;

	while ((textOut[start] == ' ') && (start < outPos))
	{
		start++;
	}

	if (start > 0)
	{
		memmove(textOut, &textOut[start], (outPos - start) + 1);
	}

	return true;
}
