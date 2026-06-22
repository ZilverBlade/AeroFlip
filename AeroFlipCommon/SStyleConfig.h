#pragma once

#include <windef.h>

#include "ETextureQuality.h"

namespace aeroflip
{
	struct SStyleConfig
	{
		BOOL bRenderWindowBorders;

		INT iHorizontalSpacingMM;
		INT iVerticalSpacingMM;

		void InitDefault()
		{
			bRenderWindowBorders = TRUE;
			iHorizontalSpacingMM = 1200;
			iVerticalSpacingMM = 1200;
		}
	};
}