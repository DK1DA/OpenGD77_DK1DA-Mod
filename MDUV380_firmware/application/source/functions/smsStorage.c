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
#include "functions/smsStorage.h"
#include "functions/smsCore.h"
#include "functions/ticks.h"
#include "functions/trx.h"
#include "hardware/SPI_Flash.h"
#include "hardware/HR-C6000.h"

#define SMS_STORAGE_MAGIC           0x4D53444F // 'ODSM'
#define SMS_STORAGE_VERSION         2
#define SMS_STORAGE_DEBOUNCE_MS     1500

typedef struct __attribute__((packed))
{
	uint32_t    magic;
	uint16_t    version;
	uint16_t    imageSize;
	uint8_t     inboxCount;
	uint8_t     sentCount;
	uint8_t     quickTextCount;
	uint8_t     options;
	uint32_t    writeCounter;
	uint32_t    crc;                // over everything after this field
	smsRecord_t inbox[SMS_INBOX_SLOTS];
	smsRecord_t sent[SMS_SENT_SLOTS];
	char        quickTexts[SMS_QUICKTEXT_SLOTS][SMS_MAX_TEXT_LENGTH + 1];
} smsStorageImage_t;

// SPI_Flash_write() can span at most two 4KB sectors, and dmrDataCRC32()
// consumes byte-swapped pairs, so the image must stay <= 8KB and even sized
typedef char assert_smsImageFitsTwoSectors[(sizeof(smsStorageImage_t) <= 8192) ? 1 : -1];
typedef char assert_smsImageSizeIsEven[((sizeof(smsStorageImage_t) % 2) == 0) ? 1 : -1];

static __attribute__((section(".ccmram"))) smsStorageImage_t storageImage;

static bool storageDirty = false;
static ticksTimer_t debounceTimer = { 0, 0 };

static const char *defaultQuickTexts[] =
{
	"QRZ?",
	"QSL 73",
	"ON MY WAY",
	"CALL ME",
	"BACK IN 5 MIN"
};

static uint32_t computeImageCRC(void)
{
	const uint8_t *afterCRC = ((const uint8_t *)&storageImage.crc) + sizeof(storageImage.crc);
	uint16_t length = sizeof(smsStorageImage_t) - (afterCRC - (const uint8_t *)&storageImage);

	return dmrDataCRC32(afterCRC, length);
}

static void formatStorage(void)
{
	memset(&storageImage, 0, sizeof(storageImage));
	storageImage.magic = SMS_STORAGE_MAGIC;
	storageImage.version = SMS_STORAGE_VERSION;
	storageImage.imageSize = sizeof(smsStorageImage_t);
	storageImage.options = (SMS_OPTION_WAIT_FOR_ACK | SMS_OPTION_OWN_ID_ONLY);

	storageImage.quickTextCount = (sizeof(defaultQuickTexts) / sizeof(defaultQuickTexts[0]));
	for (uint8_t i = 0; i < storageImage.quickTextCount; i++)
	{
		strncpy(storageImage.quickTexts[i], defaultQuickTexts[i], SMS_MAX_TEXT_LENGTH);
	}

	storageDirty = true;
	ticksTimerStart(&debounceTimer, SMS_STORAGE_DEBOUNCE_MS);
}

void smsStorageInit(void)
{
	SPI_Flash_read(SMS_STORAGE_FLASH_ADDRESS, (uint8_t *)&storageImage, sizeof(storageImage));

	if ((storageImage.magic != SMS_STORAGE_MAGIC) ||
			(storageImage.version != SMS_STORAGE_VERSION) ||
			(storageImage.imageSize != sizeof(smsStorageImage_t)) ||
			(storageImage.crc != computeImageCRC()))
	{
		formatStorage();
	}
}

void smsStorageMarkDirty(void)
{
	storageDirty = true;
	ticksTimerStart(&debounceTimer, SMS_STORAGE_DEBOUNCE_MS);
}

void smsStorageTick(void)
{
	if ((storageDirty == false) || (ticksTimerHasExpired(&debounceTimer) == false))
	{
		return;
	}

	// Writing takes ~100ms (2 sector erases + programming), so keep it away
	// from live DMR traffic and in-flight SMS work
	if (trxTransmissionEnabled || trxIsTransmitting ||
			((trxGetMode() == RADIO_MODE_DIGITAL) && (slotState != DMR_STATE_IDLE)) ||
			(smsCoreIsIdle() == false))
	{
		ticksTimerStart(&debounceTimer, SMS_STORAGE_DEBOUNCE_MS);
		return;
	}

	storageImage.writeCounter++;
	storageImage.crc = computeImageCRC();

	if (SPI_Flash_write(SMS_STORAGE_FLASH_ADDRESS, (uint8_t *)&storageImage, sizeof(storageImage)))
	{
		storageDirty = false;
	}
	else
	{
		ticksTimerStart(&debounceTimer, SMS_STORAGE_DEBOUNCE_MS); // retry later
	}
}

//
// Generic record-list helpers shared by inbox and sent box
//
static bool recordListAdd(smsRecord_t *records, uint8_t slots, uint8_t *count, uint32_t peerId, const char *text, uint8_t flags)
{
	if ((text == NULL) || (text[0] == 0))
	{
		return false;
	}

	// shift down, newest lives at index 0
	uint8_t used = *count;

	if (used > (slots - 1))
	{
		used = slots - 1;
	}

	memmove(&records[1], &records[0], used * sizeof(smsRecord_t));

	records[0].peerId = peerId;
	records[0].flags = flags;
	strncpy(records[0].text, text, SMS_MAX_TEXT_LENGTH);
	records[0].text[SMS_MAX_TEXT_LENGTH] = 0;

	if (*count < slots)
	{
		(*count)++;
	}

	smsStorageMarkDirty();

	return true;
}

static bool recordListDelete(smsRecord_t *records, uint8_t *count, uint8_t index)
{
	if (index >= *count)
	{
		return false;
	}

	memmove(&records[index], &records[index + 1], (*count - index - 1) * sizeof(smsRecord_t));
	(*count)--;
	memset(&records[*count], 0, sizeof(smsRecord_t));

	smsStorageMarkDirty();

	return true;
}

uint8_t smsInboxCount(void)
{
	return storageImage.inboxCount;
}

const smsRecord_t *smsInboxGet(uint8_t index)
{
	return ((index < storageImage.inboxCount) ? &storageImage.inbox[index] : NULL);
}

bool smsInboxAdd(uint32_t srcId, const char *text)
{
	return recordListAdd(storageImage.inbox, SMS_INBOX_SLOTS, &storageImage.inboxCount, srcId, text, SMS_RECORD_FLAG_UNREAD);
}

bool smsInboxDelete(uint8_t index)
{
	return recordListDelete(storageImage.inbox, &storageImage.inboxCount, index);
}

void smsInboxMarkRead(uint8_t index)
{
	if ((index < storageImage.inboxCount) && (storageImage.inbox[index].flags & SMS_RECORD_FLAG_UNREAD))
	{
		storageImage.inbox[index].flags &= ~SMS_RECORD_FLAG_UNREAD;
		smsStorageMarkDirty();
	}
}

uint8_t smsInboxUnreadCount(void)
{
	uint8_t unread = 0;

	for (uint8_t i = 0; i < storageImage.inboxCount; i++)
	{
		if (storageImage.inbox[i].flags & SMS_RECORD_FLAG_UNREAD)
		{
			unread++;
		}
	}

	return unread;
}

uint8_t smsSentCount(void)
{
	return storageImage.sentCount;
}

const smsRecord_t *smsSentGet(uint8_t index)
{
	return ((index < storageImage.sentCount) ? &storageImage.sent[index] : NULL);
}

bool smsSentAdd(uint32_t dstId, const char *text)
{
	return recordListAdd(storageImage.sent, SMS_SENT_SLOTS, &storageImage.sentCount, dstId, text, 0);
}

bool smsSentDelete(uint8_t index)
{
	return recordListDelete(storageImage.sent, &storageImage.sentCount, index);
}

uint8_t smsQuickTextCount(void)
{
	return storageImage.quickTextCount;
}

const char *smsQuickTextGet(uint8_t index)
{
	return ((index < storageImage.quickTextCount) ? storageImage.quickTexts[index] : NULL);
}

bool smsQuickTextSet(uint8_t index, const char *text)
{
	if ((text == NULL) || (text[0] == 0) || (index >= SMS_QUICKTEXT_SLOTS) || (index > storageImage.quickTextCount))
	{
		return false;
	}

	strncpy(storageImage.quickTexts[index], text, SMS_MAX_TEXT_LENGTH);
	storageImage.quickTexts[index][SMS_MAX_TEXT_LENGTH] = 0;

	if (index == storageImage.quickTextCount)
	{
		storageImage.quickTextCount++;
	}

	smsStorageMarkDirty();

	return true;
}

bool smsQuickTextDelete(uint8_t index)
{
	if (index >= storageImage.quickTextCount)
	{
		return false;
	}

	memmove(&storageImage.quickTexts[index], &storageImage.quickTexts[index + 1],
			(storageImage.quickTextCount - index - 1) * sizeof(storageImage.quickTexts[0]));
	storageImage.quickTextCount--;
	memset(&storageImage.quickTexts[storageImage.quickTextCount], 0, sizeof(storageImage.quickTexts[0]));

	smsStorageMarkDirty();

	return true;
}

uint8_t smsStorageGetOptions(void)
{
	return storageImage.options;
}

void smsStorageSetOptions(uint8_t options)
{
	if (storageImage.options != options)
	{
		storageImage.options = options;
		smsStorageMarkDirty();
	}
}
