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
#include "user_interface/uiGlobals.h"
#include "user_interface/menuSystem.h"
#include "user_interface/uiLocalisation.h"
#include "user_interface/uiUtilities.h"
#include "functions/smsCore.h"
#include "functions/smsStorage.h"

enum { OPTION_WAIT_FOR_ACK = 0, OPTION_OWN_ID_ONLY, NUM_SMS_OPTIONS };

static menuStatus_t menuSMSOptionsExitCode = MENU_STATUS_SUCCESS;

static void updateScreen(void);
static void handleEvent(uiEvent_t *ev);

menuStatus_t menuSMSOptions(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		menuDataGlobal.numItems = NUM_SMS_OPTIONS;
		menuDataGlobal.currentItemIndex = 0;
		updateScreen();

		return (MENU_STATUS_LIST_TYPE | MENU_STATUS_SUCCESS);
	}

	menuSMSOptionsExitCode = MENU_STATUS_SUCCESS;

	if (ev->hasEvent)
	{
		handleEvent(ev);
	}

	return menuSMSOptionsExitCode;
}

static void updateScreen(void)
{
	char entryBuffer[SCREEN_LINE_BUFFER_SIZE * 2];
	int mNum;

	displayClearBuf();
	menuDisplayTitle(currentLanguage->options);

	for (int i = MENU_START_ITERATION_VALUE; i <= MENU_END_ITERATION_VALUE; i++)
	{
		mNum = menuGetMenuOffset(NUM_SMS_OPTIONS, i);
		if (mNum == MENU_OFFSET_BEFORE_FIRST_ENTRY)
		{
			continue;
		}
		else if (mNum == MENU_OFFSET_AFTER_LAST_ENTRY)
		{
			break;
		}

		switch (mNum)
		{
			case OPTION_WAIT_FOR_ACK:
				snprintf(entryBuffer, sizeof(entryBuffer), "%s:%s",
						currentLanguage->sms_wait_for_ack, (smsOptionWaitForAck() ? currentLanguage->on : currentLanguage->off));
				break;

			case OPTION_OWN_ID_ONLY:
				snprintf(entryBuffer, sizeof(entryBuffer), "%s:%s",
						currentLanguage->sms_own_id_only, (smsOptionOwnIdOnly() ? currentLanguage->on : currentLanguage->off));
				break;

			default:
				entryBuffer[0] = 0;
				break;
		}

		menuDisplayEntry(i, mNum, entryBuffer, 0, THEME_ITEM_FG_TEXT_INPUT, THEME_ITEM_COLOUR_NONE, THEME_ITEM_BG);
	}

	displayRender();
}

static void toggleCurrentOption(void)
{
	switch (menuDataGlobal.currentItemIndex)
	{
		case OPTION_WAIT_FOR_ACK:
			smsOptionSetWaitForAck(smsOptionWaitForAck() == false);
			break;

		case OPTION_OWN_ID_ONLY:
			smsOptionSetOwnIdOnly(smsOptionOwnIdOnly() == false);
			break;
	}
}

static void handleEvent(uiEvent_t *ev)
{
	if ((ev->events & KEY_EVENT) == 0)
	{
		return;
	}

	if (KEYCHECK_SHORTUP(ev->keys, KEY_RED) || KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		menuSystemPopPreviousMenu();
		return;
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_LEFT) || KEYCHECK_PRESS(ev->keys, KEY_RIGHT))
	{
		toggleCurrentOption();
		updateScreen();
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_DOWN))
	{
		menuSystemMenuIncrement(&menuDataGlobal.currentItemIndex, NUM_SMS_OPTIONS);
		updateScreen();
	}
	else if (KEYCHECK_PRESS(ev->keys, KEY_UP))
	{
		menuSystemMenuDecrement(&menuDataGlobal.currentItemIndex, NUM_SMS_OPTIONS);
		updateScreen();
	}
}
