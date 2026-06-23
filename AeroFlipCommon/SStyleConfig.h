#pragma once

#include <windef.h>

#include "ETextureQuality.h"

namespace aeroflip
{
#define kAS_SLOW 5
#define kAS_NORMAL 12
#define kAS_FAST 24

	struct SStyleConfig
	{
		BOOL bShowDesktopWhenFlipping;
		BOOL bRenderWindowBorders;

		UINT uMaxWindowsVisible;

		INT iHorizontalSpacingMM;
		INT iVerticalSpacingMM;

		INT iAnimationSpeed;

		void InitDefault()
		{
			bShowDesktopWhenFlipping = TRUE;
			bRenderWindowBorders = TRUE;
			uMaxWindowsVisible = 5;
			iHorizontalSpacingMM = 1200;
			iVerticalSpacingMM = 600;
			iAnimationSpeed = kAS_NORMAL;
		}
	};
}