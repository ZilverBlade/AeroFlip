#pragma once

#include "SKeyBindConfig.h"
#include "SStyleConfig.h"
#include "SRendererConfig.h"

namespace aeroflip
{
	struct SAeroFlipConfig
	{
		SKeyBindConfig kbConfig;
		SStyleConfig sConfig;
		SRendererConfig rConfig;
	};
}