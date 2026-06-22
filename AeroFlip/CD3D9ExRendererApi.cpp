#include "stdafx.h" 
#include "Resource.h"
#include "SWindowDrawObject.h"
#include "CD3D9ExRendererApi.h"
#include "CWindowProvider.h"

#include <d3dx9.h> 

namespace aeroflip
{
#define DEVICE_CALL(call)                                                                   \
        if (FAILED(call))                                                                   \
            throw std::runtime_error("Failed to perform device call '"#call"'!")            \

	struct SVertex3D
	{
		float x, y, z;    // 3D Coordinates
		float u, v;       // Texture Mapping UV Coordinates
	};
#define D3DFVF_VERTEX3D (D3DFVF_XYZ | D3DFVF_TEX1)

	struct SVertex2D
	{
		float x, y, z, rhw; // Screen coordinates (z=0, rhw=1)
		float u, v;         // Texture coordinates
	};
#define D3DFVF_VERTEX2D (D3DFVF_XYZRHW | D3DFVF_TEX1)

	SVertex3D g_QuadVertices[] = {
		{ -1.0f, 1.0f, 0.0f, 0.0f, 0.0f },   // Top Left
		{ 1.0f, 1.0f, 0.0f, 1.0f, 0.0f },    // Top Right
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },  // Bottom Left
		{ 1.0f, -1.0f, 0.0f, 1.0f, 1.0f },   // Bottom Right
	};

	IDirect3DTexture9* g_pBorderTextures[8] = { NULL };
	UINT g_BorderResourceIDs[8] = {
		IDR_WINDOWBORDER_TOPLEFT,     // 0
		IDR_WINDOWBORDER_TOP,         // 1
		IDR_WINDOWBORDER_TOPRIGHT,    // 2
		IDR_WINDOWBORDER_RIGHT,       // 3
		IDR_WINDOWBORDER_BOTTOMRIGHT, // 4
		IDR_WINDOWBORDER_BOTTOM,      // 5
		IDR_WINDOWBORDER_BOTTOMLEFT,  // 6
		IDR_WINDOWBORDER_LEFT         // 7
	};

	void LoadBorderTextures(IDirect3DDevice9Ex* pDevice, HINSTANCE hInstance)
	{
		for (int i = 0; i < 8; ++i)
		{
			if (FAILED(D3DXCreateTextureFromResourceW(
				pDevice,
				hInstance,
				MAKEINTRESOURCEW(g_BorderResourceIDs[i]),
				&g_pBorderTextures[i]
				)))
			{
				throw std::runtime_error("Failed to load window border texture!");
			}
		}
	}
	void DestroyBorderTextures()
	{
		for (int i = 0; i < 8; ++i)
		{
			SafeRelease(g_pBorderTextures[i]);
		}
	}

	HRESULT CaptureWindowToTexture(const SWindowTarget* pTarget, IDirect3DDevice9Ex* pDevice, IDirect3DTexture9** ppTexture, BOOL bMipMaps)
	{
		HWND hTargetWnd = pTarget->hWnd;
		UINT width = 0;
		UINT height = 0;
		BOOL bUseRAMCache = (pTarget->bMinimized && pTarget->pCachedPixels != NULL);

		if (bUseRAMCache)
		{
			width = pTarget->uCachedWidth;
			height = pTarget->uCachedHeight;
		}
		else
		{
			if (IsIconic(hTargetWnd))
			{
				if (pTarget->bHasCachedBounds)
				{
					width = pTarget->rcCachedBounds.right - pTarget->rcCachedBounds.left;
					height = pTarget->rcCachedBounds.bottom - pTarget->rcCachedBounds.top;
				}
				else
				{
					WINDOWPLACEMENT wp;
					wp.length = sizeof(WINDOWPLACEMENT);
					GetWindowPlacement(hTargetWnd, &wp);
					width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
					height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
				}
			}
			else
			{
				RECT rect;
				GetClientRect(hTargetWnd, &rect);
				width = rect.right - rect.left;
				height = rect.bottom - rect.top;
			}

			if (width <= 0 || height <= 0)
			{
				return E_FAIL;
			}
		}

		BOOL bRecreateTexture = FALSE;
		if (*ppTexture == NULL)
		{
			bRecreateTexture = TRUE;
		}
		else
		{
			D3DSURFACE_DESC desc;
			if (FAILED((*ppTexture)->GetLevelDesc(0, &desc)))
			{
				bRecreateTexture = TRUE;
			}
			else
			{
				bRecreateTexture = (desc.Width != width || desc.Height != height);
			}
		}

		if (bRecreateTexture)
		{
			SafeRelease(*ppTexture);

			HRESULT hr = pDevice->CreateTexture(width, height, bMipMaps ? 0 : 1,
				D3DUSAGE_DYNAMIC | (bMipMaps ? D3DUSAGE_AUTOGENMIPMAP : 0), D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
				ppTexture, NULL);

			if (FAILED(hr)) return hr;
		}

		D3DLOCKED_RECT lockedRect;
		if (FAILED((*ppTexture)->LockRect(0, &lockedRect, NULL, D3DLOCK_DISCARD)))
		{
			return E_FAIL;
		}

		int nRowBytes = width * 4;
		BYTE* pDest = (BYTE*)lockedRect.pBits;

		if (bUseRAMCache)
		{
			BYTE* pSrc = pTarget->pCachedPixels;

			for (UINT y = 0; y < height; ++y)
			{
				memcpy(pDest, pSrc, nRowBytes);
				pDest += lockedRect.Pitch;
				pSrc += nRowBytes;
			}
		}
		else
		{
			HDC hdcScreen = GetDC(NULL);
			HDC hdcMem = CreateCompatibleDC(hdcScreen);
			HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
			HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);

			PrintWindow(hTargetWnd, hdcMem, 3);

			BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), (LONG)width, -(LONG)height, 1, 32, BI_RGB };

			if (lockedRect.Pitch == nRowBytes)
			{
				GetDIBits(hdcMem, hBitmap, 0, height, lockedRect.pBits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
			}
			else
			{
				BYTE* pPixels = (BYTE*)malloc(nRowBytes * height);
				GetDIBits(hdcMem, hBitmap, 0, height, pPixels, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

				BYTE* pSrc = pPixels;
				for (UINT y = 0; y < height; ++y)
				{
					memcpy(pDest, pSrc, nRowBytes);
					pDest += lockedRect.Pitch;
					pSrc += nRowBytes;
				}

				free(pPixels);
			}

			SelectObject(hdcMem, hOld);
			DeleteObject(hBitmap);
			DeleteDC(hdcMem);
			ReleaseDC(NULL, hdcScreen);
		}

		(*ppTexture)->UnlockRect(0);

		if (bMipMaps)
		{
			(*ppTexture)->SetAutoGenFilterType(D3DTEXF_LINEAR);
			(*ppTexture)->GenerateMipSubLevels();
		}
		return S_OK;
	}

	void Draw3DBorderQuad(IDirect3DDevice9Ex* pDevice, IDirect3DTexture9* pTexture,
		float x1, float y1, float x2, float y2)
	{
		if (!pTexture) return;

		SVertex3D verts[] = {
			{ x1, y1, 0.0f, 0.0f, 0.0f }, // Top Left
			{ x2, y1, 0.0f, 1.0f, 0.0f }, // Top Right
			{ x1, y2, 0.0f, 0.0f, 1.0f }, // Bottom Left
			{ x2, y2, 0.0f, 1.0f, 1.0f }  // Bottom Right
		};

		pDevice->SetTexture(0, pTexture);
		pDevice->SetFVF(D3DFVF_VERTEX3D);
		pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(SVertex3D));
	}

	CD3D9ExRendererApi::CD3D9ExRendererApi(const SRendererConfig* pConfig, HWND hWnd)
		: m_hWindow(hWnd)
	{
		memcpy(&m_Config, pConfig, sizeof(SRendererConfig));

		InitD3D9Ex();

		UINT uAdapter;
		D3DDEVTYPE devType;

		if (pConfig->bHardwareAcceleration)
		{
			uAdapter = USelectD3D9ExHardwareAdapter(&devType);
		}
		else
		{
			uAdapter = USelectD3D9ExSoftwareAdapter(&devType);
		}

		CreateD3D9ExDevice(uAdapter, devType, hWnd, pConfig->uMultiSampleLevel);

		HINSTANCE hInst = GetModuleHandle(NULL);
		LoadBorderTextures(m_pD3D9ExDevice, hInst);
	}

	CD3D9ExRendererApi::~CD3D9ExRendererApi()
	{
		DestroyBorderTextures();
		ReleaseWindows();
		SafeRelease(m_pD3D9ExDevice);
		SafeRelease(m_pD3D9Ex);
	}

	void CD3D9ExRendererApi::UpdateWindows(class CWindowProvider* pWindowProvider)
	{
		std::vector<HWND> remainingWindows;

		const SWindowTarget* pWindowTargets = NULL;
		UINT uNumWindowTargets = 0;
		pWindowProvider->QueryWindows(&pWindowTargets, &uNumWindowTargets);

		UINT uTexturesCapturedThisFrame = 0;
		const UINT uMaxCapturesPerFrame = 1;

		HWND hCandidateLiveUpdateWindow = NULL;
		if (m_Config.bLiveCapture)
		{
			if (!m_WindowTextureMap.empty())
			{
				for (auto it = m_WindowTextureMap.begin(); it != m_WindowTextureMap.end(); ++it)
				{
					if (it->second.bNextLiveUpdate)
					{
						hCandidateLiveUpdateWindow = it->first;
						it->second.bNextLiveUpdate = FALSE;
						for (;;)
						{
							if (it == m_WindowTextureMap.end())
							{
								break;
							}
							auto next = ++it;
							if (next != m_WindowTextureMap.end())
							{
								// skip minimised windows
								if (IsIconic(next->first))
								{
									continue;
								}
								next->second.bNextLiveUpdate = TRUE;
								break;
							}
							else
							{
								next = m_WindowTextureMap.begin();
							}
						}
						break;

					}
				}
				if (hCandidateLiveUpdateWindow == NULL)
				{
					m_WindowTextureMap.begin()->second.bNextLiveUpdate = TRUE;
				}
			}
		}

		for (UINT uIndex = 0; uIndex < uNumWindowTargets; ++uIndex)
		{
			const SWindowTarget* pTarget = pWindowTargets + uIndex;
			HWND hTargetWnd = pTarget->hWnd;

			auto it = m_WindowTextureMap.find(hTargetWnd);

			SWindowTexturePair* pTexturePair;

			if (it == m_WindowTextureMap.end())
			{
				pTexturePair = &m_WindowTextureMap[pTarget->hWnd];
				ZeroMemory(pTexturePair, sizeof(SWindowTexturePair));
			}
			else
			{
				pTexturePair = &it->second;
			}

			if (pTarget->bNeedsUpdate && uTexturesCapturedThisFrame < uMaxCapturesPerFrame ||
				(!pTarget->bMinimized && hCandidateLiveUpdateWindow == pTarget->hWnd))
			{
				if (SUCCEEDED(CaptureWindowToTexture(pTarget, m_pD3D9ExDevice, &pTexturePair->pD3D9Texture,
					m_Config.dwTextureQuality == eTQ_SMOOTH_MIP)))
				{
					pWindowProvider->MarkWindowUpdated(hTargetWnd);
					uTexturesCapturedThisFrame++;
				}
			}

			pTexturePair->bIsFocused = pTarget->bActive;
			remainingWindows.push_back(hTargetWnd);
		}

		for (auto it = m_WindowTextureMap.begin(); it != m_WindowTextureMap.end();)
		{
			BOOL bPersists = FALSE;
			for (auto hCandidateWnd : remainingWindows)
			{
				if (hCandidateWnd == it->first)
				{
					bPersists = TRUE;
					break;
				}
			}
			if (!bPersists)
			{
				SafeRelease(it->second.pD3D9Texture);
				it = m_WindowTextureMap.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void CD3D9ExRendererApi::ReleaseWindows()
	{
		for (auto& kv : m_WindowTextureMap)
		{
			SafeRelease(kv.second.pD3D9Texture);
		}
		m_WindowTextureMap.clear();
		ResetD3D9ExDevice();
	}

	void CD3D9ExRendererApi::OnRender(const SWindowDrawObject* pWindows, UINT cWindows)
	{
		if (!m_pD3D9ExDevice) return;

		HRESULT hr = m_pD3D9ExDevice->TestCooperativeLevel();
		if (hr == D3DERR_DEVICENOTRESET)
		{
			ResetD3D9ExDevice();
			return;
		}
		else if (FAILED(hr))
		{
			throw std::runtime_error("D3D9 device lost!");
		}

		D3DCOLOR clearColor = D3DCOLOR_ARGB(0, 0, 0, 0);
		m_pD3D9ExDevice->Clear(0, NULL, D3DCLEAR_TARGET, clearColor, 1.0f, 0);

		if (SUCCEEDED(m_pD3D9ExDevice->BeginScene()))
		{
			DEVICE_CALL(m_pD3D9ExDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
			DEVICE_CALL(m_pD3D9ExDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
			DEVICE_CALL(m_pD3D9ExDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));
			DEVICE_CALL(m_pD3D9ExDevice->SetRenderState(D3DRS_LIGHTING, FALSE));
			DEVICE_CALL(m_pD3D9ExDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));

			// TODO, maybe quality setting?

			if (m_Config.dwTextureQuality == eTQ_NEAREST)
			{
				DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 0));
				DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT));
				DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			}
			else
			{
				DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 7));
				DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC));
				DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC));
			}
			if (m_Config.dwTextureQuality == eTQ_SMOOTH_MIP)
			{
				DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR));
			}
			else
			{
				DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE));
			}

			DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
			DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
			DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP));

			DEVICE_CALL(m_pD3D9ExDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE));
			DEVICE_CALL(m_pD3D9ExDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE));
			DEVICE_CALL(m_pD3D9ExDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR));

			D3DXMATRIX matProj, matView;
			FLOAT fScreenAspect = static_cast<FLOAT>(GetSystemMetrics(SM_CXSCREEN)) / static_cast<FLOAT>(GetSystemMetrics(SM_CYSCREEN));
			D3DXMatrixPerspectiveFovLH(&matProj, D3DXToRadian(45.0f), fScreenAspect, 0.1f, 100.0f);
			DEVICE_CALL(m_pD3D9ExDevice->SetTransform(D3DTS_PROJECTION, &matProj));

			D3DXVECTOR3 origin(0.0f, 0.0f, -5.0f);
			D3DXVECTOR3 target(0.0f, 0.0f, 0.0f);
			D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
			D3DXMatrixLookAtLH(&matView, &origin, &target, &up);
			DEVICE_CALL(m_pD3D9ExDevice->SetTransform(D3DTS_VIEW, &matView));

			for (INT i = static_cast<INT>(cWindows)-1; i >= 0; --i)
			{
				IDirect3DTexture9* pTexture = NULL;
				auto it = m_WindowTextureMap.find(pWindows[i].hTargetWnd);
				if (it != m_WindowTextureMap.end())
				{
					pTexture = it->second.pD3D9Texture;
				}

				if (!pTexture || pWindows[i].fOpacity < 1.0f / 255.0f) continue;

				D3DSURFACE_DESC desc;
				pTexture->GetLevelDesc(0, &desc);

				float winW_px = static_cast<float>(desc.Width);
				float winH_px = static_cast<float>(desc.Height);
				float aspect = winW_px / winH_px;

				D3DXMATRIX matScale, matRotY, matTranslate, matWorld;
				D3DXMatrixScaling(&matScale, aspect * pWindows[i].fScale[0], pWindows[i].fScale[1], pWindows[i].fScale[2]);
				D3DXMatrixRotationY(&matRotY, D3DXToRadian(pWindows[i].fRotationY));
				D3DXMatrixTranslation(&matTranslate, pWindows[i].fPosition[0], pWindows[i].fPosition[1], pWindows[i].fPosition[2]);

				matWorld = matScale * matRotY * matTranslate;
				DEVICE_CALL(m_pD3D9ExDevice->SetTransform(D3DTS_WORLD, &matWorld));

				DEVICE_CALL(m_pD3D9ExDevice->SetTransform(D3DTS_PROJECTION, &matProj));
				DEVICE_CALL(m_pD3D9ExDevice->SetTransform(D3DTS_VIEW, &matView));
				DEVICE_CALL(m_pD3D9ExDevice->SetTransform(D3DTS_WORLD, &matWorld));

				DWORD bAlpha = static_cast<DWORD>(pWindows[i].fOpacity * 255.0f);
				if (bAlpha > 255) bAlpha = 255;
				DEVICE_CALL(m_pD3D9ExDevice->SetRenderState(D3DRS_TEXTUREFACTOR, (bAlpha << 24) | 0x00FFFFFF));

				DEVICE_CALL(m_pD3D9ExDevice->SetTexture(0, pTexture));
				DEVICE_CALL(m_pD3D9ExDevice->SetFVF(D3DFVF_VERTEX3D));
				DEVICE_CALL(m_pD3D9ExDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, g_QuadVertices, sizeof(SVertex3D)));

				if (!pWindows[i].bDesktopBg && pWindows[i].bDecorated)
				{
					float borderSizeX = 9.0f / winW_px;
					float borderSizeY = 9.0f / winH_px;
					float topBorderSizeY = 38.0f / winH_px;
					float rightBorderSizeX = 138.0f / winW_px;

					float thickLX = borderSizeX * 2.0f;       // 9px wide (Left side)
					float thickSmallRX = borderSizeX * 2.0f;  // 9px wide (Right side border)
					float thickRX = rightBorderSizeX * 2.0f;  // 138px wide (Button asset width)
					float thickBY = borderSizeY * 2.0f;       // 9px high
					float thickTY = topBorderSizeY * 2.0f;    // 38px high

					float leftEdge = -1.0f;
					float rightEdge = 1.0f;
					float topEdge = 1.0f;
					float bottomEdge = -1.0f;

					float outerRightEdge = rightEdge + thickSmallRX;

					// Top Left Corner
					Draw3DBorderQuad(m_pD3D9ExDevice, g_pBorderTextures[0], leftEdge - thickLX, topEdge + thickTY, leftEdge, topEdge);

					// Top Border
					Draw3DBorderQuad(m_pD3D9ExDevice, g_pBorderTextures[1], leftEdge, topEdge + thickTY, outerRightEdge - thickRX, topEdge);

					// Top Right Corner
					Draw3DBorderQuad(m_pD3D9ExDevice, g_pBorderTextures[2], outerRightEdge - thickRX, topEdge + thickTY, outerRightEdge, topEdge);

					// Right Border
					Draw3DBorderQuad(m_pD3D9ExDevice, g_pBorderTextures[3], rightEdge, topEdge, outerRightEdge, bottomEdge);

					// Bottom Right
					Draw3DBorderQuad(m_pD3D9ExDevice, g_pBorderTextures[4], outerRightEdge - thickRX, bottomEdge, outerRightEdge, bottomEdge - thickBY);

					// Bottom Border
					Draw3DBorderQuad(m_pD3D9ExDevice, g_pBorderTextures[5], leftEdge, bottomEdge, outerRightEdge - thickRX, bottomEdge - thickBY);

					// Bottom Left Corner
					Draw3DBorderQuad(m_pD3D9ExDevice, g_pBorderTextures[6], leftEdge - thickLX, bottomEdge, leftEdge, bottomEdge - thickBY);

					// Left Border
					Draw3DBorderQuad(m_pD3D9ExDevice, g_pBorderTextures[7], leftEdge - thickLX, topEdge, leftEdge, bottomEdge);
				}
			}

			DEVICE_CALL(m_pD3D9ExDevice->EndScene());
		}

		DEVICE_CALL(m_pD3D9ExDevice->Present(NULL, NULL, NULL, NULL));
	}

	void CD3D9ExRendererApi::InitD3D9Ex()
	{
		if (!SUCCEEDED(Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3D9Ex)))
		{
			throw std::runtime_error("Failed to init Direct3D9!");
		}
	}

	void CD3D9ExRendererApi::CreateD3D9ExDevice(UINT uAdapter, D3DDEVTYPE devType, HWND hWnd, UINT uMultiSampleLevel)
	{
		HRESULT hr = S_OK;
		DWORD dwFlags = devType == D3DDEVTYPE_HAL ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;

		ZeroMemory(&m_D3DPresentParams, sizeof(D3DPRESENT_PARAMETERS));
		m_D3DPresentParams.hDeviceWindow = hWnd;
		m_D3DPresentParams.Windowed = TRUE;
		m_D3DPresentParams.BackBufferFormat = D3DFMT_A8R8G8B8;
		m_D3DPresentParams.MultiSampleType = min((D3DMULTISAMPLE_TYPE)uMultiSampleLevel, D3DMULTISAMPLE_16_SAMPLES);
		m_D3DPresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
		m_D3DPresentParams.PresentationInterval = m_Config.bVSync ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;

		// Find closest available MSAA quality.
		while (m_D3DPresentParams.MultiSampleType > D3DMULTISAMPLE_NONE)
		{
			DWORD dwMsQualityLevels = 0;
			if (FAILED(m_pD3D9Ex->CheckDeviceMultiSampleType(uAdapter, devType, m_D3DPresentParams.BackBufferFormat,
				m_D3DPresentParams.Windowed, m_D3DPresentParams.MultiSampleType, &dwMsQualityLevels)) || dwMsQualityLevels == 0)
			{
				--*(INT*)&m_D3DPresentParams.MultiSampleType;
			}
			else
			{
				break;
			}
		}

		D3DDISPLAYMODEEX mode;
		mode.Size = sizeof(D3DDISPLAYMODEEX);
		m_pD3D9Ex->GetAdapterDisplayModeEx(uAdapter, &mode, NULL);

		hr = m_pD3D9Ex->CheckDeviceFormat(uAdapter, devType,
			mode.Format, D3DUSAGE_RENDERTARGET,
			D3DRTYPE_SURFACE, m_D3DPresentParams.BackBufferFormat);

		if (FAILED(hr))
		{
			throw std::runtime_error("A8R8G8B8 is completely unsupported on this adapter setup.");
		}

		hr = m_pD3D9Ex->CheckDeviceFormat(uAdapter, devType,
			mode.Format, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
			D3DRTYPE_TEXTURE, m_D3DPresentParams.BackBufferFormat);
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to query alpha-capable backbuffer!");
		}

		hr = m_pD3D9Ex->CreateDeviceEx(uAdapter, devType, hWnd, dwFlags,
			&m_D3DPresentParams,
			NULL,
			&m_pD3D9ExDevice);

		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to create D3D9 Device!");
		}
	}

	UINT CD3D9ExRendererApi::USelectD3D9ExHardwareAdapter(D3DDEVTYPE* pOutDevType)
	{
		OutputDebugString(L"Selecting D3D9 Hardware Accelerated Adapter\n");
		UINT uNumAdapters = m_pD3D9Ex->GetAdapterCount();
		UINT uSelectedAdapter = D3DADAPTER_DEFAULT;
		BOOL bFoundHardware = FALSE;

		for (UINT uAdapter = 0; uAdapter < uNumAdapters; ++uAdapter)
		{
			D3DCAPS9 caps;
			HRESULT hr = m_pD3D9Ex->GetDeviceCaps(uAdapter, D3DDEVTYPE_HAL, &caps);

			if (SUCCEEDED(hr))
			{
				uSelectedAdapter = uAdapter;
				bFoundHardware = true;
				OutputDebugString(L"Found D3D9 Hardware Accelerated Adapter\n");
				break;
			}
		}

		if (!bFoundHardware)
		{
			OutputDebugString(L"No hardware adapter found. Falling back to software/REF.\n");
			return USelectD3D9ExSoftwareAdapter(pOutDevType);
		}
		*pOutDevType = D3DDEVTYPE_HAL;
		return uSelectedAdapter;
	}

	UINT CD3D9ExRendererApi::USelectD3D9ExSoftwareAdapter(D3DDEVTYPE* pOutDevType)
	{
		UINT uNumAdapters = m_pD3D9Ex->GetAdapterCount();

		for (UINT uAdapter = 0; uAdapter < uNumAdapters; ++uAdapter)
		{
			D3DCAPS9 caps;
			HRESULT hr = m_pD3D9Ex->GetDeviceCaps(uAdapter, D3DDEVTYPE_REF, &caps);

			if (SUCCEEDED(hr))
			{
				OutputDebugString(L"Successfully selected D3D9 Software (Reference) Adapter.\n");
				*pOutDevType = D3DDEVTYPE_REF;
				return uAdapter;
			}
		}

		throw std::runtime_error("Failed to find an adapter supporting the Reference Rasterizer!");
	}

	void CD3D9ExRendererApi::ResetD3D9ExDevice()
	{
		if (FAILED(m_pD3D9ExDevice->Reset(&m_D3DPresentParams)))
		{
			throw std::runtime_error("Failed to reset D3D9 device!");
		}
	}
}