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
#include <stdlib.h>
#include "user_interface/uiGlobals.h"
#include "user_interface/menuSystem.h"
#include "user_interface/uiLocalisation.h"
#include "user_interface/uiUtilities.h"
#include "io/keyboard.h"
#include "functions/smsCore.h"
#include "functions/smsStorage.h"

#define DEST_DIGITS_MAX          8 // 16777215 is the largest 24-bit DMR ID
#define TEXT_CHARS_PER_LINE      (DISPLAY_SIZE_X / 8)
#define TEXT_VISIBLE_LINES       5
#define TEXT_AREA_Y              (FONT_SIZE_3_HEIGHT * 2)

enum { FIELD_DESTINATION = 0, FIELD_TEXT };

static char composeText[SMS_MAX_TEXT_LENGTH + 1];
static char destDigits[DEST_DIGITS_MAX + 1];
static int textPos = 0;
static int focusedField = FIELD_TEXT;
static int quickTextEditSlot = -1;

// prefill is written straight into the compose buffers, consumed on first run
static bool prefillPending = false;
static int pendingQuickTextEditSlot = -1;

// set while the contact picker is open, so returning from it preserves the
// typed text and only moves to the message field when a contact was chosen
static bool pickerReturnPending = false;
static bool pickerSelected = false;

static menuStatus_t menuSMSComposeExitCode = MENU_STATUS_SUCCESS;

static void updateScreen(void);
static void handleEvent(uiEvent_t *ev);

void menuSMSComposePrefill(uint32_t dstId, const char *text)
{
	destDigits[0] = 0;
	composeText[0] = 0;

	if (dstId != 0)
	{
		snprintf(destDigits, sizeof(destDigits), "%u", (unsigned int)dstId);
	}

	if (text != NULL)
	{
		strncpy(composeText, text, SMS_MAX_TEXT_LENGTH);
		composeText[SMS_MAX_TEXT_LENGTH] = 0;
	}

	prefillPending = true;
}

void menuSMSComposeSetDestination(uint32_t dstId)
{
	if (dstId != 0)
	{
		snprintf(destDigits, sizeof(destDigits), "%u", (unsigned int)dstId);
	}

	// a contact was chosen in the picker: on return, keep the text and move
	// the focus to the message field
	pickerSelected = true;
}

void menuSMSComposeSetQuickTextEditTarget(int slotIndex)
{
	pendingQuickTextEditSlot = slotIndex;
}

menuStatus_t menuSMSCompose(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		quickTextEditSlot = pendingQuickTextEditSlot;
		pendingQuickTextEditSlot = -1;

		if (quickTextEditSlot >= 0)
		{
			const char *quickText = smsQuickTextGet(quickTextEditSlot);

			composeText[0] = 0;
			destDigits[0] = 0;

			if (quickText != NULL)
			{
				strncpy(composeText, quickText, SMS_MAX_TEXT_LENGTH);
				composeText[SMS_MAX_TEXT_LENGTH] = 0;
			}

			focusedField = FIELD_TEXT;
		}
		else if (pickerReturnPending)
		{
			// returning from the contact picker: keep the typed text and
			// recipient; only advance to the message field if a contact was
			// actually selected (a cancel stays on the recipient field)
			if (pickerSelected)
			{
				focusedField = FIELD_TEXT;
			}

			pickerReturnPending = false;
			pickerSelected = false;
		}
		else
		{
			if (prefillPending == false)
			{
				composeText[0] = 0;
				destDigits[0] = 0;
			}

			focusedField = ((destDigits[0] == 0) ? FIELD_DESTINATION : FIELD_TEXT);
		}

		textPos = strlen(composeText);
		prefillPending = false;
		updateScreen();

		return (MENU_STATUS_INPUT_TYPE | MENU_STATUS_SUCCESS);
	}

	menuSMSComposeExitCode = MENU_STATUS_SUCCESS;

	if (ev->hasEvent)
	{
		handleEvent(ev);
	}

	return menuSMSComposeExitCode;
}

static void updateScreen(void)
{
	char lineBuffer[TEXT_CHARS_PER_LINE + 1];

	displayClearBuf();
	menuDisplayTitle((quickTextEditSlot >= 0) ? currentLanguage->quick_text : currentLanguage->new_message);

	keypadAlphaEnable = (focusedField == FIELD_TEXT);

	// destination line (hidden when editing a quick text template)
	if (quickTextEditSlot < 0)
	{
		snprintf(lineBuffer, sizeof(lineBuffer), "%s:%s%s",
				currentLanguage->send_to, destDigits,
				((focusedField == FIELD_DESTINATION) ? "_" : ""));
		displayPrintAt(0, FONT_SIZE_3_HEIGHT, lineBuffer, FONT_SIZE_3);
	}

	// text area with a scrolling window that follows the cursor
	int textLength = strlen(composeText);
	int cursorLine = textPos / TEXT_CHARS_PER_LINE;
	int firstLine = ((cursorLine >= TEXT_VISIBLE_LINES) ? (cursorLine - TEXT_VISIBLE_LINES + 1) : 0);

	for (int line = 0; line < TEXT_VISIBLE_LINES; line++)
	{
		int offset = (firstLine + line) * TEXT_CHARS_PER_LINE;

		if (offset > textLength)
		{
			break;
		}

		int chunk = textLength - offset;

		if (chunk > TEXT_CHARS_PER_LINE)
		{
			chunk = TEXT_CHARS_PER_LINE;
		}

		memcpy(lineBuffer, &composeText[offset], chunk);
		lineBuffer[chunk] = 0;
		displayPrintAt(0, TEXT_AREA_Y + (line * FONT_SIZE_3_HEIGHT), lineBuffer, FONT_SIZE_3);
	}

	if (focusedField == FIELD_TEXT)
	{
		int cursorX = (textPos % TEXT_CHARS_PER_LINE) * 8;
		int cursorY = TEXT_AREA_Y + ((cursorLine - firstLine + 1) * FONT_SIZE_3_HEIGHT) - 2;

		displayDrawFastHLine(cursorX, cursorY, 8, true);
	}

	// footer: remaining characters
	snprintf(lineBuffer, sizeof(lineBuffer), "%d", SMS_MAX_TEXT_LENGTH - textLength);
	displayPrintAt(DISPLAY_SIZE_X - (strlen(lineBuffer) * 8), DISPLAY_SIZE_Y - FONT_SIZE_3_HEIGHT, lineBuffer, FONT_SIZE_3);

	displayRender();
}

static void sendOrSave(void)
{
	if (quickTextEditSlot >= 0)
	{
		if (composeText[0] != 0)
		{
			smsQuickTextSet(quickTextEditSlot, composeText);
			keypadAlphaEnable = false;
			menuSystemPopPreviousMenu();
		}
		else
		{
			menuSMSComposeExitCode |= MENU_STATUS_ERROR;
		}

		return;
	}

	uint32_t dstId = (uint32_t)strtoul(destDigits, NULL, 10);

	if ((dstId == 0) || (composeText[0] == 0))
	{
		menuSMSComposeExitCode |= MENU_STATUS_ERROR;
		return;
	}

	if (smsSendMessage(dstId, composeText))
	{
		keypadAlphaEnable = false;
		menuSystemPopAllAndDisplayRootMenu(); // progress is reported via notifications
	}
	else
	{
		menuSMSComposeExitCode |= MENU_STATUS_ERROR; // busy or not in DMR mode
	}
}

static void handleEvent(uiEvent_t *ev)
{
	if (ev->events & KEY_EVENT)
	{
		if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
		{
			keypadAlphaEnable = false;
			menuSystemPopPreviousMenu();
			return;
		}
		else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
		{
			if (quickTextEditSlot >= 0)
			{
				sendOrSave(); // save the quick-text template
			}
			else if (focusedField == FIELD_DESTINATION)
			{
				// in the recipient field, GREEN opens the contact picker;
				// preserve any typed text/recipient whether the picker returns
				// a selection or is cancelled
				keypadAlphaEnable = false;
				pickerReturnPending = true;
				pickerSelected = false;
				menuSystemPushNewMenu(MENU_SMS_CONTACT);
			}
			else
			{
				sendOrSave(); // send the message
			}
			return;
		}
		else if (KEYCHECK_PRESS(ev->keys, KEY_UP) && (quickTextEditSlot < 0))
		{
			focusedField = FIELD_DESTINATION;
			updateScreen();
			return;
		}
		else if (KEYCHECK_PRESS(ev->keys, KEY_DOWN))
		{
			focusedField = FIELD_TEXT;
			updateScreen();
			return;
		}

		if (focusedField == FIELD_DESTINATION)
		{
			if (KEYCHECK_PRESS(ev->keys, KEY_LEFT))
			{
				int len = strlen(destDigits);

				if (len > 0)
				{
					destDigits[len - 1] = 0;
				}

				updateScreen();
			}
			else
			{
				int keyval = menuGetKeypadKeyValue(ev, true);

				if ((keyval != 99) && (strlen(destDigits) < DEST_DIGITS_MAX))
				{
					char digit[2] = { (char)('0' + keyval), 0 };

					strcat(destDigits, digit);
					updateScreen();
				}
			}
		}
		else // FIELD_TEXT
		{
			if (KEYCHECK_PRESS(ev->keys, KEY_LEFT))
			{
				moveCursorLeftInString(composeText, &textPos, BUTTONCHECK_DOWN(ev, BUTTON_SK2));
				updateScreen();
			}
			else if (KEYCHECK_PRESS(ev->keys, KEY_RIGHT))
			{
				moveCursorRightInString(composeText, &textPos, SMS_MAX_TEXT_LENGTH, BUTTONCHECK_DOWN(ev, BUTTON_SK2));
				updateScreen();
			}
			else if ((ev->keys.event & (KEY_MOD_PREVIEW | KEY_MOD_PRESS)) && (ev->keys.key >= 32) && (ev->keys.key <= 126))
			{
				if (textPos < SMS_MAX_TEXT_LENGTH)
				{
					bool atEnd = (composeText[textPos] == 0);

					composeText[textPos] = ev->keys.key;

					if (atEnd)
					{
						composeText[textPos + 1] = 0;
					}

					if ((ev->keys.event & KEY_MOD_PRESS) && (textPos < (SMS_MAX_TEXT_LENGTH - 1)))
					{
						textPos++;
					}

					updateScreen();
				}
			}
		}
	}
}
