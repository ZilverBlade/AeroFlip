// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <winuser.h>
#include <d3d9.h>
#include <dxgi.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#define SafeRelease(interface) \
    if ((interface) != NULL) { \
        (interface)->Release(); \
        (interface) = NULL; \
    }

// C++ standard libraries
#include <stdexcept>