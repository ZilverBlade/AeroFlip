#include "stdafx.h" 
#include "SWindowDrawObject.h"
#include "CD3D9ExRendererApi.h"
#include "CWindowProvider.h"

#include <d3dx9.h> 

namespace aeroflip
{
#define DEVICE_CALL(call)															\
		if (FAILED(call)) 															\
			throw std::runtime_error("Failed to perform device call '"#call"'!")	\

	struct SVertex3D
	{
		float x, y, z;    // 3D Coordinates
		float u, v;       // Texture Mapping UV Coordinates
	};
#define D3DFVF_VERTEX3D (D3DFVF_XYZ | D3DFVF_TEX1)

	SVertex3D g_QuadVertices[] = {
		{ -1.0f, 1.0f, 0.0f, 0.0f, 0.0f }, // Top Left
		{ 1.0f, 1.0f, 0.0f, 1.0f, 0.0f }, // Top Right
		{ -1.0f, -1.0f, 0.0f, 0.0f, 1.0f }, // Bottom Left
		{ 1.0f, -1.0f, 0.0f, 1.0f, 1.0f }, // Bottom Right
	};

	HRESULT CaptureWindowToTexture(const SWindowTarget* pTarget, IDirect3DDevice9Ex* pDevice, IDirect3DTexture9** ppTexture)
	{
		HWND hTargetWnd = pTarget->hWnd;
		UINT width = 0;
		UINT height = 0;
		BOOL bUseRAMCache = (pTarget->bMinimized && pTarget->pCachedPixels != NULL);
		// mip maps are super slow :/
		const BOOL bMipMaps = FALSE;

		if (bUseRAMCache)
		{
			width = pTarget->uCachedWidth;
			height = pTarget->uCachedHeight;
		}
		else
		{
			if (IsIconic(hTargetWnd))
			{
				WINDOWPLACEMENT wp;
				wp.length = sizeof(WINDOWPLACEMENT);
				GetWindowPlacement(hTargetWnd, &wp);
				width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
				height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
			}
			else
			{
				RECT rect;
				GetWindowRect(hTargetWnd, &rect);
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
				D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
				ppTexture, NULL);

			if (FAILED(hr)) return hr;
		}

		D3DLOCKED_RECT lockedRect;
		if (FAILED((*ppTexture)->LockRect(0, &lockedRect, NULL, D3DLOCK_DISCARD)))
		{
			return E_FAIL;
		}

		int rowBytes = width * 4;
		BYTE* pDest = (BYTE*)lockedRect.pBits;

		if (bUseRAMCache)
		{
			BYTE* pSrc = pTarget->pCachedPixels;

			for (UINT y = 0; y < height; ++y)
			{
				memcpy(pDest, pSrc, rowBytes);
				pDest += lockedRect.Pitch;
				pSrc += rowBytes;
			}
		}
		else
		{
			HDC hdcScreen = GetDC(NULL);
			HDC hdcMem = CreateCompatibleDC(hdcScreen);
			HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
			HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);

			PrintWindow(hTargetWnd, hdcMem, 2);

			BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), width, -(LONG)height, 1, 32, BI_RGB };

			if (lockedRect.Pitch == rowBytes)
			{
				GetDIBits(hdcMem, hBitmap, 0, height, lockedRect.pBits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
			}
			else
			{
				std::vector<BYTE> tempPixels(rowBytes * height);
				GetDIBits(hdcMem, hBitmap, 0, height, tempPixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

				BYTE* pSrc = tempPixels.data();
				for (UINT y = 0; y < height; ++y)
				{
					memcpy(pDest, pSrc, rowBytes);
					pDest += lockedRect.Pitch;
					pSrc += rowBytes;
				}
			}

			SelectObject(hdcMem, hOld);
			DeleteObject(hBitmap);
			DeleteDC(hdcMem);
			ReleaseDC(NULL, hdcScreen);
		}

		(*ppTexture)->UnlockRect(0);

		if (bMipMaps)
		{
			DEVICE_CALL(D3DXFilterTexture(*ppTexture, NULL, 0, D3DX_DEFAULT));
		}
		return S_OK;
	}

	CD3D9ExRendererApi::CD3D9ExRendererApi(const SD3D9ExRendererApiConfig* pConfig)
		: m_hWindow(pConfig->hWnd)
	{
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

		CreateD3D9ExDevice(uAdapter, devType, pConfig->hWnd, pConfig->uMultiSampleLevel);
	}
	CD3D9ExRendererApi::~CD3D9ExRendererApi()
	{
		SafeRelease(m_pD3D9ExDevice);
		SafeRelease(m_pD3D9Ex);
		for (auto& texture : m_WindowTextureList)
		{
			SafeRelease(texture.pD3D9Texture);
		}
	}

	void CD3D9ExRendererApi::UpdateWindows(class CWindowProvider* pWindowProvider)
	{
		std::vector<HWND> remainingWindows;

		const SWindowTarget* pWindowTargets = NULL;
		UINT uNumWindowTargets = 0;
		pWindowProvider->QueryWindows(&pWindowTargets, &uNumWindowTargets);

		UINT uTexturesCapturedThisFrame = 0;
		const UINT uMaxCapturesPerFrame = 4;

		for (UINT uIndex = 0; uIndex < uNumWindowTargets; ++uIndex)
		{
			const SWindowTarget* pTarget = pWindowTargets + uIndex;
			HWND hTargetWnd = pTarget->hWnd;

			auto it = m_WindowTextureList.begin();
			if (!m_WindowTextureList.empty())
			{
				while (it != m_WindowTextureList.end())
				{
					if (it->hWnd == hTargetWnd) break;
					++it;
				}
			}

			SWindowTexturePair* pTexturePair;

			if (m_WindowTextureList.empty() || it == m_WindowTextureList.end())
			{
				SWindowTexturePair texturePair;
				ZeroMemory(&texturePair, sizeof(SWindowTexturePair));
				texturePair.hWnd = hTargetWnd;
				m_WindowTextureList.push_back(texturePair);
				pTexturePair = &m_WindowTextureList.back();
			}
			else
			{
				pTexturePair = &*it;
			}

			if (pTarget->bNeedsUpdate)
			{
				if (uTexturesCapturedThisFrame < uMaxCapturesPerFrame)
				{
					if (SUCCEEDED(CaptureWindowToTexture(pTarget, m_pD3D9ExDevice, &pTexturePair->pD3D9Texture)))
					{
						pWindowProvider->MarkWindowUpdated(hTargetWnd);
						uTexturesCapturedThisFrame++;
					}
				}
			}

			pTexturePair->bIsFocused = pTarget->bActive;
			remainingWindows.push_back(hTargetWnd);
		}

		for (auto it = m_WindowTextureList.begin(); it != m_WindowTextureList.end();)
		{
			BOOL bPersists = FALSE;
			for (auto hCandidateWnd : remainingWindows)
			{
				if (hCandidateWnd == it->hWnd)
				{
					bPersists = TRUE;
					break;
				}
			}
			if (!bPersists)
			{
				SafeRelease(it->pD3D9Texture);
				it = m_WindowTextureList.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void CD3D9ExRendererApi::ReleaseWindows()
	{
		for (auto& texture : m_WindowTextureList)
		{
			SafeRelease(texture.pD3D9Texture);
		}
		m_WindowTextureList.clear();
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
			DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 7));
			DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC));
			DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC));
			DEVICE_CALL(m_pD3D9ExDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR));

			D3DXMATRIX matProj, matView;
			D3DXMatrixPerspectiveFovLH(&matProj, D3DXToRadian(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
			DEVICE_CALL(m_pD3D9ExDevice->SetTransform(D3DTS_PROJECTION, &matProj));

			D3DXVECTOR3 origin(0.0f, 0.0f, -5.0f);
			D3DXVECTOR3 target(0.0f, 0.0f, 0.0f);
			D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
			D3DXMatrixLookAtLH(&matView, &origin, &target, &up);
			DEVICE_CALL(m_pD3D9ExDevice->SetTransform(D3DTS_VIEW, &matView));

			for (INT i = static_cast<INT>(cWindows)-1; i >= 0; --i)
			{
				IDirect3DTexture9* pTexture = NULL;
				for (UINT uWindowIndex = 0; uWindowIndex < (UINT)m_WindowTextureList.size(); ++uWindowIndex)
				{
					if (m_WindowTextureList[uWindowIndex].hWnd == pWindows[i].hTargetWnd)
					{
						pTexture = m_WindowTextureList[uWindowIndex].pD3D9Texture;
						break;
					}
				}

				if (!pTexture || pWindows[i].fOpacity <= 0.001f) continue;

				D3DSURFACE_DESC desc;
				pTexture->GetLevelDesc(0, &desc);
				float aspect = static_cast<float>(desc.Width) / static_cast<float>(desc.Height);

				D3DXMATRIX matScale, matRotY, matTranslate, matWorld;
				D3DXMatrixScaling(&matScale, aspect * pWindows[i].fScale[0], pWindows[i].fScale[1], pWindows[i].fScale[2]);
				D3DXMatrixRotationY(&matRotY, D3DXToRadian(pWindows[i].fRotationY));
				D3DXMatrixTranslation(&matTranslate, pWindows[i].fPosition[0], pWindows[i].fPosition[1], pWindows[i].fPosition[2]);

				matWorld = matScale * matRotY * matTranslate;
				DEVICE_CALL(m_pD3D9ExDevice->SetTransform(D3DTS_WORLD, &matWorld));

				DWORD alphaVal = static_cast<DWORD>(pWindows[i].fOpacity * 255.0f) << 24;
				DEVICE_CALL(m_pD3D9ExDevice->SetRenderState(D3DRS_TEXTUREFACTOR, alphaVal | 0x00FFFFFF));

				DEVICE_CALL(m_pD3D9ExDevice->SetTexture(0, pTexture));
				DEVICE_CALL(m_pD3D9ExDevice->SetFVF(D3DFVF_VERTEX3D));
				DEVICE_CALL(m_pD3D9ExDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, g_QuadVertices, sizeof(SVertex3D)));
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
		m_D3DPresentParams.MultiSampleType = (D3DMULTISAMPLE_TYPE)uMultiSampleLevel;
		m_D3DPresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
		m_D3DPresentParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

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
		static HRESULT reference = D3DERR_INVALIDCALL;
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