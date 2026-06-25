#pragma once

#include <windef.h>

#include "EFlipShortcutMode.h"

namespace aeroflip
{
	struct SKeyBindConfig
	{
		BOOL bShiftToMoveBack;
		BOOL bCycleOnFirstTab;
		BOOL bPressKeyAgainToExit;

		DWORD dwFlipShortcutMode;

		void InitDefault()
		{
			bShiftToMoveBack = TRUE;
			bCycleOnFirstTab = FALSE;
			bPressKeyAgainToExit = FALSE;
			dwFlipShortcutMode = eFSM_WIN_TAB;
		}
	};
}