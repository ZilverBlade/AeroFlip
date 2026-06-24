#pragma once

#include <windef.h>
#include <shlobj.h>

#include <stdexcept>
#include <string>

// Relative to %LocalAppData%
#define AF_LOCAL_DIR TEXT("\\ZilverBlade\\AeroFlip")

namespace aeroflip
{
	// Creates directory if doesnt exist yet 
	LPCTSTR GetConfigIniPath()
	{
		static TCHAR szPath[MAX_PATH];
		ZeroMemory(szPath, sizeof(szPath));

		if (FAILED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath)))
		{
			throw std::runtime_error("Failed to get %LocalAppData%");
		}

		lstrcat(szPath, AF_LOCAL_DIR);
		int nError = SHCreateDirectoryEx(NULL, szPath, NULL);
		if (nError != ERROR_ALREADY_EXISTS && nError != ERROR_SUCCESS)
		{
			throw std::runtime_error("Failed to create %LocalAppData%/ZilverBlade/AeroFlip directory!");
		}

		lstrcat(szPath, TEXT("\\config.ini"));

		return szPath;
	}


#define IniPath
#define IniSection 
#define IniRead 0
#define IniWrite 1
#define ReadIniCfg(key, pValue) (*pValue) = (INT)GetPrivateProfileInt(IniSection, key, *pValue, IniPath)
#define WriteIniCfg(key, value) WritePrivateProfileString(IniSection, key, std::to_wstring((INT)(value)).c_str(), IniPath)

#define DefIniCfg(mm, kk, vv) \
	do {\
if (mm == IniWrite) \
WriteIniCfg(kk, (*vv));\
else \
ReadIniCfg(kk, vv); } while (0)

	void DefineIni(int nMode, SAeroFlipConfig* pCfg)
	{
		LPCTSTR lpszIniPath = aeroflip::GetConfigIniPath();
#undef IniPath 
#undef IniSection 
#define IniPath lpszIniPath

		{
#define IniSection TEXT("KeyBind")
			DefIniCfg(nMode, TEXT("CycleOnFirstTab"), &pCfg->kbConfig.bCycleOnFirstTab);
			DefIniCfg(nMode, TEXT("ShiftToMoveBack"), &pCfg->kbConfig.bShiftToMoveBack);
			DefIniCfg(nMode, TEXT("FlipShortcutMode"), &pCfg->kbConfig.dwFlipShortcutMode);
#undef IniSection 
		}

	{
#define IniSection TEXT("Style")
		DefIniCfg(nMode, TEXT("ShowDesktopWhenFlipping"), &pCfg->sConfig.bShowDesktopWhenFlipping);
		DefIniCfg(nMode, TEXT("RenderWindowBorders"), &pCfg->sConfig.bRenderWindowBorders);
		DefIniCfg(nMode, TEXT("MaxWindowsVisible"), &pCfg->sConfig.uMaxWindowsVisible);
		DefIniCfg(nMode, TEXT("HorizontalSpacingMM"), &pCfg->sConfig.iHorizontalSpacingMM);
		DefIniCfg(nMode, TEXT("VerticalSpacingMM"), &pCfg->sConfig.iVerticalSpacingMM);
		DefIniCfg(nMode, TEXT("AnimationSpeed"), &pCfg->sConfig.iAnimationSpeed);
#undef IniSection 
	}

	{
#define IniSection TEXT("Renderer")
		DefIniCfg(nMode, TEXT("HardwareAcceleration"), &pCfg->rConfig.bHardwareAcceleration);
		DefIniCfg(nMode, TEXT("LiveCapture"), &pCfg->rConfig.bLiveCapture);
		DefIniCfg(nMode, TEXT("PersistInVRAM"), &pCfg->rConfig.bPersistInVRAM);
		DefIniCfg(nMode, TEXT("VSync"), &pCfg->rConfig.bVSync);
		DefIniCfg(nMode, TEXT("MultiSampleLevel"), &pCfg->rConfig.uMultiSampleLevel);
		DefIniCfg(nMode, TEXT("RendererMode"), &pCfg->rConfig.dwRendererMode);
		DefIniCfg(nMode, TEXT("TextureQuality"), &pCfg->rConfig.dwTextureQuality);
#undef IniSection 
	}

#undef IniPath 
	}

}