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
 * SMS recipient picker: lists the private-call (individual) DMR contacts from
 * the codeplug. GREEN selects one as the message destination and returns to the
 * compose screen; RED cancels.
 */
#include "user_interface/uiGlobals.h"
#include "user_interface/menuSystem.h"
#include "user_interface/uiLocalisation.h"
#include "user_interface/uiUtilities.h"
#include "functions/codeplug.h"

static menuStatus_t menuSMSContactExitCode = MENU_STATUS_SUCCESS;

static void updateScreen(void);
static void handleEvent(uiEvent_t *ev);

// Fetch the private-call contact for the given list row.
static bool getContactForRow(int row, CodeplugContact_t *contact)
{
	return (codeplugContactGetDataForNumberInType(row + 1, CONTACT_CALLTYPE_PC, contact) > 0);
}

menuStatus_t menuSMSContact(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
		menuDataGlobal.numItems = codeplugContactsGetCount(CONTACT_CALLTYPE_PC);
		menuDataGlobal.currentItemIndex = 0;
		updateScreen();

		return (MENU_STATUS_LIST_TYPE | MENU_STATUS_SUCCESS);
	}

	menuSMSContactExitCode = MENU_STATUS_SUCCESS;

	if (ev->hasEvent)
	{
		handleEvent(ev);
	}

	return menuSMSContactExitCode;
}

static void updateScreen(void)
{
	char entryBuffer[SCREEN_LINE_BUFFER_SIZE * 2];
	char nameBuffer[SCREEN_LINE_BUFFER_SIZE];
	CodeplugContact_t contact;
	int mNum;

	displayClearBuf();
	menuDisplayTitle(currentLanguage->contacts);

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

		if (getContactForRow(mNum, &contact))
		{
			codeplugUtilConvertBufToString(contact.name, nameBuffer, 16);
			snprintf(entryBuffer, sizeof(entryBuffer), "%s", nameBuffer);
		}
		else
		{
			entryBuffer[0] = 0;
		}

		menuDisplayEntry(i, mNum, entryBuffer, 0, THEME_ITEM_FG_TEXT_INPUT, THEME_ITEM_COLOUR_NONE, THEME_ITEM_BG);
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
		return;
	}
	else if (KEYCHECK_SHORTUP(ev->keys, KEY_GREEN))
	{
		if (menuDataGlobal.numItems > 0)
		{
			CodeplugContact_t contact;

			if (getContactForRow(menuDataGlobal.currentItemIndex, &contact))
			{
				// tgNumber is the plain DMR ID; codeplugContactGetPackedId()
				// would OR the call-type flag into the top byte, which is not a
				// valid destination ID.
				menuSMSComposeSetDestination(contact.tgNumber);
				menuSystemPopPreviousMenu(); // back to compose, recipient filled in
				return;
			}
		}

		menuSMSContactExitCode |= MENU_STATUS_ERROR;
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
