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
 * Persistent store for the DMR SMS feature: inbox, sent messages and
 * quick-text templates, held as a single RAM image and lazily persisted
 * to a dedicated SPI flash region.
 *
 * Physical flash map on this platform (16MB W25Q128):
 *   0x000000 - 0x00FFFF  emulated EEPROM (settings)
 *   0x010000             calibration copy
 *   0x020000 - ~0x100000 codeplug (FLASH_ADDRESS_OFFSET 128KB + codeplug regions)
 *   0x050000 / 0x0D8000  DMR ID databases
 *   0x0AF400 / 0x100000  voice prompts
 *   0xC00000             << SMS store (this module, 2 sectors) >>
 *   0xE00000 - 0xFFFFFF  GPS log (last 2MB)
 */
#ifndef _OPENGD77_SMS_STORAGE_H_
#define _OPENGD77_SMS_STORAGE_H_

#include <stdint.h>
#include <stdbool.h>
#include "functions/dmrDataProtocol.h"

#define SMS_STORAGE_FLASH_ADDRESS   0xC00000
#define SMS_INBOX_SLOTS             8
#define SMS_SENT_SLOTS              8
#define SMS_QUICKTEXT_SLOTS         8

#define SMS_RECORD_FLAG_UNREAD      0x01

// smsStorageOptions bits
#define SMS_OPTION_WAIT_FOR_ACK     0x01
#define SMS_OPTION_OWN_ID_ONLY      0x02

typedef struct
{
	uint32_t peerId;                       // sender (inbox) or destination (sent)
	uint8_t  flags;
	char     text[SMS_MAX_TEXT_LENGTH + 1];
} smsRecord_t;

void smsStorageInit(void);
void smsStorageTick(void);       // debounced persist, main loop context only
void smsStorageMarkDirty(void);

uint8_t smsInboxCount(void);
const smsRecord_t *smsInboxGet(uint8_t index);        // index 0 = newest
bool smsInboxAdd(uint32_t srcId, const char *text);   // newest-first, oldest dropped when full
bool smsInboxDelete(uint8_t index);
void smsInboxMarkRead(uint8_t index);
uint8_t smsInboxUnreadCount(void);

uint8_t smsSentCount(void);
const smsRecord_t *smsSentGet(uint8_t index);
bool smsSentAdd(uint32_t dstId, const char *text);
bool smsSentDelete(uint8_t index);

uint8_t smsQuickTextCount(void);
const char *smsQuickTextGet(uint8_t index);
bool smsQuickTextSet(uint8_t index, const char *text); // index == smsQuickTextCount() appends
bool smsQuickTextDelete(uint8_t index);

uint8_t smsStorageGetOptions(void);
void smsStorageSetOptions(uint8_t options);

#endif /* _OPENGD77_SMS_STORAGE_H_ */
