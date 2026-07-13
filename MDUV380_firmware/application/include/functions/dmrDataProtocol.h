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
 * DMR short-data (SMS) frame and payload codec.
 *
 * Pure functions only: no hardware access, no OS calls, no mutable globals,
 * so the whole module can be compiled and unit-tested on the host.
 *
 * The HR-C6000 baseband performs BPTC(196,96)/FEC and the on-air CRC in
 * hardware; this module only deals with the 12-byte logical payload of each
 * burst (CSBK preamble, data header, rate-1/2 data block) and with the
 * IP/UDP-encapsulated text payload spread across the data blocks.
 *
 * Message layouts follow ETSI TS 102 361-1/-2 plus the two de-facto text
 * message formats: Motorola TMS (UDP port 4007) and "DMR standard"/Anytone
 * (UDP port 5016), as documented in KY4YI's "DMR SMS/Text Message
 * Specification rev 1.0".
 */
#ifndef _OPENGD77_DMR_DATA_PROTOCOL_H_
#define _OPENGD77_DMR_DATA_PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>

#define DMR_DATA_BURST_LENGTH               12 // logical payload bytes per CSBK / header / rate-1/2 block burst
#define DMR_DATA_RATE34_BLOCK_LENGTH        18 // logical payload bytes per rate-3/4 block burst
#define DMR_DATA_CONFIRMED_PREFIX_LENGTH     2 // confirmed-data blocks start with a serial + CRC9 prefix

// DMR data types (high nibble of HR-C6000 register 0x51 on RX / written to register 0x50 on TX)
#define DMR_DATA_TYPE_CSBK                0x03
#define DMR_DATA_TYPE_DATA_HEADER         0x06
#define DMR_DATA_TYPE_RATE_12_DATA        0x07
#define DMR_DATA_TYPE_RATE_34_DATA        0x08

// SMS text limits. We compose up to 160 characters (in line with typical
// OEM radio limits): 38 (IP+UDP+TMS headers) + 320 + 4 (CRC32) = 362 bytes
// => 31 rate-1/2 blocks. On receive, up to 42 blocks are accepted so that
// longer messages from other vendors still decode (text beyond
// SMS_MAX_TEXT_LENGTH is truncated).
#define SMS_MAX_TEXT_LENGTH                160
#define SMS_MAX_TX_DATA_BLOCKS              31
#define SMS_MAX_DATA_BLOCKS                 42
#define SMS_MAX_PAYLOAD_LENGTH             (SMS_MAX_DATA_BLOCKS * DMR_DATA_BURST_LENGTH)

#define SMS_UDP_PORT_MOTOROLA           0x0FA7 // 4007
#define SMS_UDP_PORT_STANDARD           0x1398 // 5016
#define SMS_TEXT_OFFSET_MOTOROLA            38 // IP(20) + UDP(8) + TMS header(10)
#define SMS_TEXT_OFFSET_STANDARD            32 // IP(20) + UDP(8) + vendor header(4)

typedef enum
{
	SMS_ENCODE_OK = 0,
	SMS_ENCODE_EMPTY,
	SMS_ENCODE_TOO_LONG
} smsEncodeResult_t;

typedef struct
{
	uint32_t srcId;
	uint32_t dstId;
	uint8_t  dpf;           // Data Packet Format (0x01 response, 0x02 unconfirmed, 0x03 confirmed)
	uint8_t  sap;           // Service Access Point (0x04 = UDP/IP)
	uint8_t  blockCount;    // blocks to follow
	uint8_t  padOctets;
	bool     isGroup;
	bool     responseRequested;
	bool     isResponsePdu; // delivery confirmation (DPF 0x01, no payload blocks)
} dmrDataHeader_t;

//
// Checksums
//
uint16_t dmrDataCRC16(const uint8_t *data, uint16_t length);                       // CRC16-CCITT, poly 0x1021, init 0x0000 (unmasked)
uint32_t dmrDataCRC32(const uint8_t *data, uint16_t length);                       // ETSI DMR CRC32, input taken as byte-swapped 16-bit pairs
uint16_t dmrDataIPHeaderChecksum(const uint8_t *ipHeader);                         // RFC 791, over 20 bytes with checksum field zeroed
uint16_t dmrDataUDPChecksum(const uint8_t *ipPacket, uint16_t udpLength);          // RFC 768, with IPv4 pseudo header

//
// 12-byte burst payload builders (CRC computed and masked internally)
//
void dmrDataBuildPreambleCSBK(uint8_t frame[DMR_DATA_BURST_LENGTH], uint32_t dstId, uint32_t srcId, uint8_t blocksToFollow);
void dmrDataBuildUnconfirmedHeader(uint8_t frame[DMR_DATA_BURST_LENGTH], uint32_t dstId, uint32_t srcId,
		uint8_t blockCount, uint8_t padOctets, bool requestResponse);
void dmrDataBuildResponseHeader(uint8_t frame[DMR_DATA_BURST_LENGTH], uint32_t dstId, uint32_t srcId, bool repeaterProfile);

//
// 12-byte burst payload parser
//
bool dmrDataParseHeader(const uint8_t frame[DMR_DATA_BURST_LENGTH], dmrDataHeader_t *header);

//
// Text payload (IP/UDP encapsulation spread over the data blocks)
//
// Builds the complete Motorola TMS payload: IP + UDP + TMS headers, UTF-16LE
// text, zero padding to a 12-byte boundary and trailing CRC32. On success
// *payloadLength is a multiple of 12 and *padOctets is the number of zero
// octets between the text and the CRC32 trailer.
smsEncodeResult_t smsPayloadBuildMotorola(uint8_t *payload, uint16_t *payloadLength, uint8_t *padOctets,
		uint32_t dstId, uint32_t srcId, const char *text, uint16_t ipSequenceNumber);

// Strict parsers for the two known formats. textOut is NUL terminated ASCII
// (non-representable characters become '?').
bool smsPayloadParseMotorola(const uint8_t *payload, uint16_t payloadLength, char *textOut, uint16_t textOutSize);
bool smsPayloadParseStandard(const uint8_t *payload, uint16_t payloadLength, char *textOut, uint16_t textOutSize);

// Lenient fallback: find the longest plausible UTF-16 printable run anywhere
// in the payload (tries little-endian first, then big-endian). Returns false
// when nothing readable (>= 4 characters) was found.
bool smsPayloadScanText(const uint8_t *payload, uint16_t payloadLength, char *textOut, uint16_t textOutSize);

#endif /* _OPENGD77_DMR_DATA_PROTOCOL_H_ */
