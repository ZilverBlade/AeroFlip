#pragma once

#include <windef.h>

#include "EFlipShortcutMode.h"

namespace aeroflip
{
	struct SKeyBindConfig
	{
		BOOL bShiftToMoveBack;
		BOOL bCycleOnFirstTab;

		DWORD dwFlipShortcutMode;

		void InitDefault()
		{
			bShiftToMoveBack = TRUE;
			bCycleOnFirstTab = FALSE;
			dwFlipShortcutMode = eFSM_WIN_TAB;
		}
	};
}