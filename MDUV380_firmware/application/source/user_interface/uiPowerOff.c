/*
 * Copyright (C) 2019-2025 Roger Clark, VK3KYY / G4KYF
 *                         Daniel Caujolle-Bert, F1RMB
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
#include "user_interface/uiGlobals.h"
#include "user_interface/menuSystem.h"
#include "user_interface/uiLocalisation.h"
#include "functions/ticks.h"
#if defined(PLATFORM_MD9600) || defined(PLATFORM_MD380) || defined(PLATFORM_MDUV380) || defined(PLATFORM_RT84_DM1701) || defined(PLATFORM_MD2017)
#include "interfaces/batteryAndPowerManagement.h"
#include "hardware/radioHardwareInterface.h"
#endif

const uint32_t POWEROFF_DURATION_MILLISECONDS = 500;

#if defined(HAS_COLOURS)
static void crtShutdownAnimation(void);
#else
static uint32_t initialEventTime;
static void updateScreen(void);
static void handleEvent(uiEvent_t *ev);
#endif

menuStatus_t uiPowerOff(uiEvent_t *ev, bool isFirstRun)
{
	if (isFirstRun)
	{
#if defined(HAS_SOFT_VOLUME)
		if (uiNotificationIsVisible() && (uiNotificationGetId() == NOTIFICATION_ID_VOLUME))
		{
			uiNotificationHide(false);
		}
#endif

#if defined(HAS_COLOURS)
		// Old-CRT-style collapse of whatever is currently on screen; replaces
		// the "Power Off" / 73 message, then shuts down.
		crtShutdownAnimation();
		powerOffFinalStage(false, false);
#else
		updateScreen();
		initialEventTime = ev->time;
#endif
	}
	else
	{
#if !defined(HAS_COLOURS)
		handleEvent(ev);
#endif
	}
	return MENU_STATUS_SUCCESS;
}

#if defined(HAS_COLOURS)
static void crtShutdownAnimation(void)
{
	const int W = DISPLAY_SIZE_X;
	const int H = DISPLAY_SIZE_Y;
	const int cx = W / 2;
	const int cy = H / 2;
	const int VSTEPS = 12;
	const int HSTEPS = 12;
	const uint16_t white = 0xFFFF;

	displayRestorePrimaryScreenBuffer();

	uint16_t *fb = displayGetPrimaryScreenBuffer();
	uint16_t *snap = uiNotificationGetOffscreenBuffer();

	// snapshot the screen as it is right now
	memcpy(snap, fb, W * H * sizeof(uint16_t));

	// Phase 1: squeeze the whole image vertically into a thin central band.
	for (int step = 1; step <= VSTEPS; step++)
	{
		int bandH = H - ((H - 2) * step) / VSTEPS; // 128 -> 2
		int top = cy - (bandH / 2);

		memset(fb, 0, W * H * sizeof(uint16_t));

		for (int dy = 0; dy < bandH; dy++)
		{
			int srcY = (dy * H) / bandH;

			if (srcY >= H)
			{
				srcY = H - 1;
			}
			memcpy(&fb[(top + dy) * W], &snap[srcY * W], W * sizeof(uint16_t));
		}

		displayRender();
		vTaskDelay((15 / portTICK_PERIOD_MS));
	}

	// Phase 2: the band becomes a bright line that collapses horizontally.
	for (int step = 1; step <= HSTEPS; step++)
	{
		int lineW = W - ((W - 2) * step) / HSTEPS; // 160 -> 2
		int left = cx - (lineW / 2);

		memset(fb, 0, W * H * sizeof(uint16_t));

		for (int dx = 0; dx < lineW; dx++)
		{
			fb[((cy - 1) * W) + (left + dx)] = white;
			fb[(cy * W) + (left + dx)] = white;
		}

		displayRender();
		vTaskDelay((15 / portTICK_PERIOD_MS));
	}

	// Phase 3: a brief central dot, then black.
	memset(fb, 0, W * H * sizeof(uint16_t));
	fb[((cy - 1) * W) + (cx - 1)] = white;
	fb[((cy - 1) * W) + cx] = white;
	fb[(cy * W) + (cx - 1)] = white;
	fb[(cy * W) + cx] = white;
	displayRender();
	vTaskDelay((70 / portTICK_PERIOD_MS));

	memset(fb, 0, W * H * sizeof(uint16_t));
	displayRender();
}
#endif

#if !defined(HAS_COLOURS)
static void updateScreen(void)
{
	const bool dblHeight = false;
	uint16_t fontHeight = (FONT_SIZE_3_HEIGHT * (dblHeight ? 2 : 1));

	displayClearBuf();
	displayThemeApply(THEME_ITEM_FG_DECORATION, THEME_ITEM_BG_NOTIFICATION);
	displayDrawRoundRectWithDropShadow(4, 4, 120 + DISPLAY_H_EXTRA_PIXELS, DISPLAY_SIZE_Y - 6, 5, true);

	displayThemeApply(THEME_ITEM_FG_WARNING_NOTIFICATION, THEME_ITEM_BG_NOTIFICATION);
	displayPrintCenteredDoubleHeight(((DISPLAY_SIZE_Y / 3) - (fontHeight / 2)), currentLanguage->power_off, FONT_SIZE_3, dblHeight);
	displayPrintCenteredDoubleHeight((((DISPLAY_SIZE_Y / 3) * 2) - (fontHeight / 2)), "73", FONT_SIZE_3, dblHeight);
	displayThemeResetToDefault();
	displayRender();
}

static void handleEvent(uiEvent_t *ev)
{
	if (
#if defined(PLATFORM_MD9600)
			(LedRead(LED_GREEN) == 0) &&
#elif defined(PLATFORM_MD380) || defined(PLATFORM_MDUV380) || defined(PLATFORM_RT84_DM1701) || defined(PLATFORM_MD2017)
			powerRotarySwitchIsOn() &&
#elif defined(PLATFORM_GD77) || defined(PLATFORM_GD77S) || defined(PLATFORM_DM1801) || defined(PLATFORM_DM1801A) // not for RD5R
			(GPIO_PinRead(GPIO_Power_Switch, Pin_Power_Switch) == 0) &&
#endif
			(batteryVoltage > CUTOFF_VOLTAGE_LOWER_HYST)
	)
	{
		// I think this is to handle if the power button is turned back on during shutdown
		menuSystemPopPreviousMenu();
		initialEventTime = 0; // Reset timeout
		return;
	}

	if ((ev->time - initialEventTime) > POWEROFF_DURATION_MILLISECONDS)
	{
		powerOffFinalStage(false, false);
	}

}
#endif
