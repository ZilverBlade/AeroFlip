#pragma once

#include <windef.h>

#include "ETextureQuality.h"

namespace aeroflip
{
	struct SStyleConfig
	{
		BOOL bRenderWindowBorders;

		FLOAT fHorizontalSpacing;
		FLOAT fVerticalSpacing;

		void InitDefault()
		{
			bRenderWindowBorders = TRUE;
			fHorizontalSpacing = 1.2f;
			fVerticalSpacing = 1.2f;
		}
	};
}