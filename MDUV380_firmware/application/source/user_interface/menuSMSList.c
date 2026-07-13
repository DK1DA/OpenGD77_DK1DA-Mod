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
 * Message list (inbox and sent box) and single-message view.
 * One implementation serves MENU_SMS_INBOX, MENU_SMS_SENT and MENU_SMS_VIEW.
 */
#include "user_interface/uiGlobals.h"
#include "user_interface/menuSystem.h"
#include "user_interface/uiLocalisation.h"
#include "user_interface/uiUtilities.h"
#include "functions/smsCore.h"
#include "functions/smsStorage.h"

#define TEXT_CHARS_PER_LINE      (DISPLAY_SIZE_X / 8)
#define VIEW_TEXT_LINES          ((DISPLAY_SIZE_Y / FONT_SIZE_3_HEIGHT) - 2) // minus title and sender line

static bool viewIsSentBox = false;
static uint8_t viewIndex = 0;
static int viewScrollLine = 0;

static menuStatus_t menuSMSListExitCode = MENU_STATUS_SUCCESS;

static void updateListScreen(bool isSentBox);
static void updateViewScreen(void);
static void handleListEvent(uiEvent_t *ev, bool isSentBox);
static void handleViewEvent(uiEvent_t *ev);
static bool deleteMessageCallback(void);

void menuSMSListSetViewTarget(bool sentBox, uint8_t recordIndex)
{
	viewIsSentBox = sentBox;
	viewIndex = recordIndex;
	viewScrollLine = 0;
}

static const smsRecord_t *getRecord(bool isSentBox, uint8_t index)
{
	return (isSentBox ? smsSentGet(index) : smsInboxGet(index));
}

static uint8_t getRecordCount(bool isSentBox)
{
	return (isSentBox ? smsSentCount() : smsInboxCount());
}

static void peerNameLookup(uint32_t peerId, char *buffer, size_t bufferLen)
{
	char nameBuf[SCREEN_LINE_BUFFER_SIZE];

	if (contactIDLookup(peerId, CONTACT_CALLTYPE_PC, nameBuf) == false)
	{
		dmrIdDataStruct_t record;

		if (dmrIDLookup(peerId, &record))
		{
			strncpy(nameBuf, record.text, sizeof(nameBuf) - 1);
			nameBuf[sizeof(nameBuf) - 1] = 0;
		}
		else
		{
			snprintf(nameBuf, sizeof(nameBuf), "%u", (unsigned int)peerId);
		}
	}

	// only keep the callsign / first word for list display
	char *space = strchr(nameBuf, ' ');

	if (space != NULL)
	{
		*space = 0;
	}

	strncpy(buffer, nameBuf, bufferLen - 1);
	buffer[bufferLen - 1] = 0;
}

menuStatus_t menuSMSList(uiEvent_t *ev, bool isFirstRun)
{
	int currentMenu = menuSystemGetCurrentMenuNumber();
	bool isView = (currentMenu == MENU_SMS_VIEW);
	bool isSentBox = ((currentMenu == MENU_SMS_SENT) || (isView && viewIsSentBox));

	if (isFirstRun)
	{
		if (isView)
		{
			if (viewIndex >= getRecordCount(viewIsSentBox))
			{
				menuSystemPopPreviousMenu();
				return MENU_STATUS_SUCCESS;
			}

			if (viewIsSentBox == false)
			{
				smsInboxMarkRead(viewIndex);
			}

			updateViewScreen();
		}
		else
		{
			menuDataGlobal.numItems = getRecordCount(isSentBox);
			menuDataGlobal.currentItemIndex = 0;
			updateListScreen(isSentBox);
		}

		return (MENU_STATUS_LIST_TYPE | MENU_STATUS_SUCCESS);
	}

	menuSMSListExitCode = MENU_STATUS_SUCCESS;

	if (isView)
	{
		// the record can vanish underneath us (deleted via the confirm box)
		if (viewIndex >= getRecordCount(viewIsSentBox))
		{
			menuSystemPopPreviousMenu();
			return MENU_STATUS_SUCCESS;
		}

		if (ev->hasEvent)
		{
			handleViewEvent(ev);
		}
	}
	else
	{
		if (ev->hasEvent)
		{
			handleListEvent(ev, isSentBox);
		}
	}

	return menuSMSListExitCode;
}

static void updateListScreen(bool isSentBox)
{
	char entryBuffer[SCREEN_LINE_BUFFER_SIZE * 2];
	char nameBuffer[SCREEN_LINE_BUFFER_SIZE];
	int mNum;

	displayClearBuf();
	menuDisplayTitle(isSentBox ? currentLanguage->sent_box : currentLanguage->inbox);

	if (menuDataGlobal.numItems == 0)
	{
		displayPrintCentered(DISPLAY_SIZE_Y / 2, currentLanguage->none, FONT_SIZE_3);
	}

	for (int i = MENU_START_ITERATION_VALUE; i <= MENU_END_ITERATION_VALUE; i++)
	{
		if (menuDataGlobal.numItems == 0)
		{
			break;
		}

		mNum = menuGetMenuOffset(menuDataGlobal.numItems, i);
		if (mNum == MENU_OFFSET_BEFORE_FIRST_ENTRY)
		{
			continue;
		}
		else if (mNum == MENU_OFFSET_AFTER_LAST_ENTRY)
		{
			break;
		}

		const smsRecord_t *record = getRecord(isSentBox, (uint8_t)mNum);

		if (record == NULL)
		{
			break;
		}

		peerNameLookup(record->peerId, nameBuffer, sizeof(nameBuffer));
		snprintf(entryBuffer, sizeof(entryBuffer), "%s%s:%s",
				((record->flags & SMS_RECORD_FLAG_UNREAD) ? "*" : ""), nameBuffer, record->text);

		menuDisplayEntry(i, mNum, entryBuffer, 0, THEME_ITEM_FG_TEXT_INPUT, THEME_ITEM_COLOUR_NONE, THEME_ITEM_BG);
	}

	displayRender();
}

static void updateViewScreen(void)
{
	char lineBuffer[TEXT_CHARS_PER_LINE + 1];
	const smsRecord_t *record = getRecord(viewIsSentBox, viewIndex);

	if (record == NULL)
	{
		return;
	}

	displayClearBuf();

	peerNameLookup(record->peerId, lineBuffer, sizeof(lineBuffer));
	menuDisplayTitle(lineBuffer);

	snprintf(lineBuffer, sizeof(lineBuffer), "%s%u", (viewIsSentBox ? ">" : "<"), (unsigned int)record->peerId);
	displayPrintAt(0, FONT_SIZE_3_HEIGHT, lineBuffer, FONT_SIZE_3);

	int textLength = strlen(record->text);
	int totalLines = (textLength + TEXT_CHARS_PER_LINE - 1) / TEXT_CHARS_PER_LINE;

	if (viewScrollLine > (totalLines - VIEW_TEXT_LINES))
	{
		viewScrollLine = totalLines - VIEW_TEXT_LINES;
	}

	if (viewScrollLine < 0)
	{
		viewScrollLine = 0;
	}

	for (int line = 0; line < VIEW_TEXT_LINES; line++)
	{
		int offset = (viewScrollLine + line) * TEXT_CHARS_PER_LINE;

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

static void handleListEvent(uiEvent_t *ev, bool isSentBox)
{
	if ((ev->events & KEY_EVENT) == 0)
	{
		return;
	}

	if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
	{
		menuSystemPopPreviousMenu();
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		if (menuDataGlobal.numItems > 0)
		{
			menuSMSListSetViewTarget(isSentBox, (uint8_t)menuDataGlobal.currentItemIndex);
			menuSystemPushNewMenu(MENU_SMS_VIEW);
		}
		return;
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_DOWN))
	{
		menuSystemMenuIncrement(&menuDataGlobal.currentItemIndex, menuDataGlobal.numItems);
		updateListScreen(isSentBox);
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
	{
		menuSystemMenuDecrement(&menuDataGlobal.currentItemIndex, menuDataGlobal.numItems);
		updateListScreen(isSentBox);
	}
}

static bool deleteMessageCallback(void)
{
	if (uiDataGlobal.MessageBox.keyPressed == KEY_GREEN)
	{
		if (viewIsSentBox)
		{
			smsSentDelete(viewIndex);
		}
		else
		{
			smsInboxDelete(viewIndex);
		}
	}

	return true;
}

static void handleViewEvent(uiEvent_t *ev)
{
	if ((ev->events & KEY_EVENT) == 0)
	{
		return;
	}

	if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
	{
		if (BUTTONCHECK_DOWN(ev, BUTTON_SK2))
		{
			const smsRecord_t *record = getRecord(viewIsSentBox, viewIndex);

			if (record != NULL)
			{
				snprintf(uiDataGlobal.MessageBox.message, MESSAGEBOX_MESSAGE_LEN_MAX, "%s", currentLanguage->delete_message);
				uiDataGlobal.MessageBox.type = MESSAGEBOX_TYPE_INFO;
				uiDataGlobal.MessageBox.buttons = MESSAGEBOX_BUTTONS_YESNO;
				uiDataGlobal.MessageBox.decoration = MESSAGEBOX_DECORATION_FRAME;
				uiDataGlobal.MessageBox.validatorCallback = deleteMessageCallback;
				menuSystemPushNewMenu(UI_MESSAGE_BOX);
			}
		}
		else
		{
			menuSystemPopPreviousMenu();
		}
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		const smsRecord_t *record = getRecord(viewIsSentBox, viewIndex);

		if (record != NULL)
		{
			// inbox: reply to the sender; sent box: re-edit and resend
			menuSMSComposePrefill(record->peerId, (viewIsSentBox ? record->text : NULL));
			menuSystemPushNewMenu(MENU_SMS_COMPOSE);
		}
		return;
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_DOWN))
	{
		viewScrollLine++;
		updateViewScreen();
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
	{
		viewScrollLine--;
		updateViewScreen();
	}
}
