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
 * Incoming message popup: shown over the channel/VFO screen when a new
 * message arrives. GREEN opens it, SK2+GREEN starts a reply, RED dismisses.
 */
#include "user_interface/uiGlobals.h"
#include "user_interface/menuSystem.h"
#include "user_interface/uiLocalisation.h"
#include "user_interface/uiUtilities.h"
#include "functions/smsCore.h"
#include "functions/smsStorage.h"

#define TEXT_CHARS_PER_LINE      (DISPLAY_SIZE_X / 8)
#define PREVIEW_LINES            3

static void updateScreen(void);
static void handleEvent(uiEvent_t *ev);
static void smsAlertTone(void);

menuStatus_t uiSMSPopup(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		smsAcknowledgeRxMessage();

		if (smsInboxGet(0) == NULL)
		{
			menuSystemPopPreviousMenu();
			return MENU_STATUS_SUCCESS;
		}

		updateScreen();
		smsAlertTone();

		return MENU_STATUS_SUCCESS;
	}

	// a newer message replaces the popup content
	if (smsHasUnseenRxMessage())
	{
		smsAcknowledgeRxMessage();
		updateScreen();
		smsAlertTone();
	}

	if (ev->hasEvent)
	{
		handleEvent(ev);
	}

	return MENU_STATUS_SUCCESS;
}

static void smsAlertTone(void)
{
	if ((voicePromptsIsPlaying() == false) && (melody_play == NULL) && (audioAmpGetStatus() == AUDIO_AMP_CHANNEL_NONE))
	{
		soundSetMelody(MELODY_SMS_RX);
	}
}

static void updateScreen(void)
{
	char lineBuffer[TEXT_CHARS_PER_LINE + 1];
	char nameBuffer[SCREEN_LINE_BUFFER_SIZE];
	const smsRecord_t *record = smsInboxGet(0);

	if (record == NULL)
	{
		return;
	}

	displayClearBuf();
	menuDisplayTitle(currentLanguage->message_from);

	if (contactIDLookup(record->peerId, CONTACT_CALLTYPE_PC, nameBuffer) == false)
	{
		dmrIdDataStruct_t idRecord;

		if (dmrIDLookup(record->peerId, &idRecord))
		{
			strncpy(nameBuffer, idRecord.text, sizeof(nameBuffer) - 1);
			nameBuffer[sizeof(nameBuffer) - 1] = 0;
		}
		else
		{
			snprintf(nameBuffer, sizeof(nameBuffer), "%u", (unsigned int)record->peerId);
		}
	}

	displayPrintCentered(FONT_SIZE_3_HEIGHT, nameBuffer, FONT_SIZE_3);

	int textLength = strlen(record->text);

	for (int line = 0; line < PREVIEW_LINES; line++)
	{
		int offset = line * TEXT_CHARS_PER_LINE;

		if (offset >= textLength)
		{
			break;
		}

		int chunk = textLength - offset;

		if (chunk > TEXT_CHARS_PER_LINE)
		{
			chunk = TEXT_CHARS_PER_LINE;
		}

		memcpy(lineBuffer, &record->text[offset], chunk);
		lineBuffer[chunk] = 0;
		displayPrintAt(0, (FONT_SIZE_3_HEIGHT * 2) + (line * FONT_SIZE_3_HEIGHT), lineBuffer, FONT_SIZE_3);
	}

	displayRender();
}

static void handleEvent(uiEvent_t *ev)
{
	if ((ev->events & KEY_EVENT) == 0)
	{
		return;
	}

	if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
	{
		menuSystemPopPreviousMenu();
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		menuSystemPopPreviousMenu();

		if (BUTTONCHECK_DOWN(ev, BUTTON_SK2))
		{
			const smsRecord_t *record = smsInboxGet(0);

			if (record != NULL)
			{
				menuSMSComposePrefill(record->peerId, NULL);
				menuSystemPushNewMenu(MENU_SMS_COMPOSE);
			}
		}
		else
		{
			smsInboxMarkRead(0);
			menuSMSListSetViewTarget(false, 0);
			menuSystemPushNewMenu(MENU_SMS_VIEW);
		}
	}
}
