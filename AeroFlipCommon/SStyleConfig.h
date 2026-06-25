#pragma once

#include <windef.h>

#include "EWindowFrameStyle.h"

namespace aeroflip
{
#define kAS_SLOW 5
#define kAS_NORMAL 12
#define kAS_FAST 24

	struct SStyleConfig
	{
		BOOL bShowDesktopWhenFlipping;

		UINT uDesktopDimmingPercent;

		UINT uMaxWindowsVisible;

		INT iHorizontalSpacingMM;
		INT iVerticalSpacingMM;

		DWORD dwWindowFrameStyle;
		INT iAnimationSpeed;

		void InitDefault()
		{
			bShowDesktopWhenFlipping = TRUE;
			uDesktopDimmingPercent = 20;
			uMaxWindowsVisible = 5;
			iHorizontalSpacingMM = 1200;
			iVerticalSpacingMM = 600;
			dwWindowFrameStyle = eWFS_WINDOWS_AERO;
			iAnimationSpeed = kAS_NORMAL;
		}
	};
}