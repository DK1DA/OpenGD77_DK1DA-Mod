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
 * Quick text templates: pick one to start a message, SK2+GREEN edits the
 * slot, SK2+RED deletes it. The last entry creates a new template while
 * free slots remain.
 */
#include "user_interface/uiGlobals.h"
#include "user_interface/menuSystem.h"
#include "user_interface/uiLocalisation.h"
#include "user_interface/uiUtilities.h"
#include "functions/smsCore.h"
#include "functions/smsStorage.h"

static menuStatus_t menuSMSQuickTextExitCode = MENU_STATUS_SUCCESS;

static void updateScreen(void);
static void handleEvent(uiEvent_t *ev);
static bool deleteQuickTextCallback(void);

static bool hasNewEntry(void)
{
	return (smsQuickTextCount() < SMS_QUICKTEXT_SLOTS);
}

static int totalEntries(void)
{
	return (smsQuickTextCount() + (hasNewEntry() ? 1 : 0));
}

menuStatus_t menuSMSQuickText(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		menuDataGlobal.numItems = totalEntries();
		menuDataGlobal.currentItemIndex = 0;
		updateScreen();

		return (MENU_STATUS_LIST_TYPE | MENU_STATUS_SUCCESS);
	}

	menuSMSQuickTextExitCode = MENU_STATUS_SUCCESS;

	// re-sync after returning from the editor
	if (menuDataGlobal.numItems != totalEntries())
	{
		menuDataGlobal.numItems = totalEntries();

		if (menuDataGlobal.currentItemIndex >= menuDataGlobal.numItems)
		{
			menuDataGlobal.currentItemIndex = (menuDataGlobal.numItems > 0) ? (menuDataGlobal.numItems - 1) : 0;
		}

		updateScreen();
	}

	if (ev->hasEvent)
	{
		handleEvent(ev);
	}

	return menuSMSQuickTextExitCode;
}

static void updateScreen(void)
{
	int mNum;

	displayClearBuf();
	menuDisplayTitle(currentLanguage->quick_text);

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

		const char *text = smsQuickTextGet((uint8_t)mNum);

		menuDisplayEntry(i, mNum, ((text != NULL) ? text : currentLanguage->new_message), 0,
				THEME_ITEM_FG_TEXT_INPUT, THEME_ITEM_COLOUR_NONE, THEME_ITEM_BG);
	}

	displayRender();
}

static bool deleteQuickTextCallback(void)
{
	if (uiDataGlobal.MessageBox.keyPressed == KEY_GREEN)
	{
		smsQuickTextDelete((uint8_t)menuDataGlobal.currentItemIndex);
	}

	return true;
}

static void handleEvent(uiEvent_t *ev)
{
	if ((ev->events & KEY_EVENT) == 0)
	{
		return;
	}

	bool onExistingEntry = (menuDataGlobal.currentItemIndex < smsQuickTextCount());

	if (KEYCHECK_SHORTUP(ev->keys, KEY_RED))
	{
		if (BUTTONCHECK_DOWN(ev, BUTTON_SK2) && onExistingEntry)
		{
			snprintf(uiDataGlobal.MessageBox.message, MESSAGEBOX_MESSAGE_LEN_MAX, "%s", currentLanguage->delete_message);
			uiDataGlobal.MessageBox.type = MESSAGEBOX_TYPE_INFO;
			uiDataGlobal.MessageBox.buttons = MESSAGEBOX_BUTTONS_YESNO;
			uiDataGlobal.MessageBox.decoration = MESSAGEBOX_DECORATION_FRAME;
			uiDataGlobal.MessageBox.validatorCallback = deleteQuickTextCallback;
			menuSystemPushNewMenu(UI_MESSAGE_BOX);
		}
		else
		{
			menuSystemPopPreviousMenu();
		}
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		if (BUTTONCHECK_DOWN(ev, BUTTON_SK2) || (onExistingEntry == false))
		{
			// edit the slot (or create a new one)
			menuSMSComposeSetQuickTextEditTarget(menuDataGlobal.currentItemIndex);
			menuSystemPushNewMenu(MENU_SMS_COMPOSE);
		}
		else
		{
			// use the template for a new message
			menuSMSComposePrefill(0, smsQuickTextGet((uint8_t)menuDataGlobal.currentItemIndex));
			menuSystemPushNewMenu(MENU_SMS_COMPOSE);
		}
		return;
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_DOWN))
	{
		menuSystemMenuIncrement(&menuDataGlobal.currentItemIndex, menuDataGlobal.numItems);
		updateScreen();
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
	{
		menuSystemMenuDecrement(&menuDataGlobal.currentItemIndex, menuDataGlobal.numItems);
		updateScreen();
	}
}
