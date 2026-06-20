#include "stdafx.h" 
#include "CRendererApi.h"
#include "CWindowProvider.h"

#include <d3dx9.h> 

#include <stdexcept>

namespace aeroflip {
#define DEVICE_CALL(call)															\
		if (FAILED(call)) 															\
			throw std::runtime_error("Failed to perform device call '"#call"'!")	\

	struct SVertex3D {
		float x, y, z;    // 3D Coordinates
		float u, v;       // Texture Mapping UV Coordinates
	};
#define D3DFVF_VERTEX3D (D3DFVF_XYZ | D3DFVF_TEX1)

	SVertex3D g_QuadVertices[] = {
		{ -1.5f, 1.0f, 0.0f, 0.0f, 0.0f }, // Top Left
		{ 1.5f, 1.0f, 0.0f, 1.0f, 0.0f }, // Top Right
		{ -1.5f, -1.0f, 0.0f, 0.0f, 1.0f }, // Bottom Left
		{ 1.5f, -1.0f, 0.0f, 1.0f, 1.0f }, // Bottom Right
	};

	HRESULT CaptureWindowToTexture(HWND hTargetWnd, IDirect3DDevice9Ex* pDevice, IDirect3DTexture9** ppTexture) {
		RECT rect;
		GetWindowRect(hTargetWnd, &rect);
		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;
		if (width <= 0 || height <= 0) return E_FAIL;

		HDC hdcScreen = GetDC(NULL);
		HDC hdcMem = CreateCompatibleDC(hdcScreen);
		HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
		HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);

		PrintWindow(hTargetWnd, hdcMem, 2);

		BOOL bRecreateTexture = FALSE;

		if (*ppTexture == NULL) {
			bRecreateTexture = TRUE;
		}
		else {
			D3DSURFACE_DESC desc;
			if (FAILED((*ppTexture)->GetLevelDesc(0, &desc))) {
				bRecreateTexture = TRUE;
			}
			bRecreateTexture = desc.Width != (UINT)width || desc.Height != (UINT)height;
		}
		if (bRecreateTexture) {
			SafeRelease(*ppTexture);
			HRESULT hr = pDevice->CreateTexture(width, height, 1,
				D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
				ppTexture, NULL);
			if (FAILED(hr)) {
				return hr;
			}
		}

		D3DLOCKED_RECT lockedRect;
		if (SUCCEEDED((*ppTexture)->LockRect(0, &lockedRect, NULL, D3DLOCK_DISCARD))) {
			BITMAPINFOHEADER bi = { sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB };
			GetDIBits(hdcMem, hBitmap, 0, height, lockedRect.pBits, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
			(*ppTexture)->UnlockRect(0);
		}

		SelectObject(hdcMem, hOld);
		DeleteObject(hBitmap);
		DeleteDC(hdcMem);
		ReleaseDC(NULL, hdcScreen);

		return S_OK;
	}

	CRendererApi::CRendererApi(const SRendererApiConfig* pConfig) : m_hWindow(pConfig->hWnd) {
		InitD3D9();

		UINT uAdapter;
		D3DDEVTYPE devType;

		if (pConfig->bHardwareAcceleration) {
			uAdapter = USelectD3D9HardwareAdapter(&devType);
		}
		else {
			uAdapter = USelectD3D9SoftwareAdapter(&devType);
		}

		CreateD3D9Device(uAdapter, devType, pConfig->hWnd, pConfig->uMultiSampleLevel);
	}
	CRendererApi::~CRendererApi() {
		SafeRelease(m_pD3D9Device);
		SafeRelease(m_pD3D9);
		for (auto kv : m_WindowTextureMap) {
			SafeRelease(kv.second);
		}
	}

	void CRendererApi::UpdateWindows(class CWindowProvider* pWindowProvider) {
		UINT index = 0;
		std::vector<HWND> remainingWindows;
		while (HWND hTargetWnd = pWindowProvider->HEnumWindows(&index)) {
			auto it = m_WindowTextureMap.find(hTargetWnd);
			if (it == m_WindowTextureMap.end()) {
				CaptureWindowToTexture(hTargetWnd, m_pD3D9Device, &m_WindowTextureMap[hTargetWnd]);
			}
			else {
				CaptureWindowToTexture(hTargetWnd, m_pD3D9Device, &it->second);
			}
			remainingWindows.push_back(hTargetWnd);
		}
		for (auto it = m_WindowTextureMap.begin(); it != m_WindowTextureMap.end(); ++it) {
			BOOL bPersists = FALSE;
			for (auto hCandidateWnd : remainingWindows) {
				if (hCandidateWnd == it->first) {
					bPersists = TRUE;
					break;
				}
			}
			if (!bPersists) {
				SafeRelease(it->second);
				it = m_WindowTextureMap.erase(it);
			}
		}
	}

	void CRendererApi::OnRender(FLOAT fDeltaTime) {
		UNREFERENCED_PARAMETER(fDeltaTime);
		if (!m_pD3D9Device) return;

		HRESULT hr = m_pD3D9Device->TestCooperativeLevel();
		if (hr == D3DERR_DEVICENOTRESET) {
			ResetD3D9Device();
			return;
		}
		else if (FAILED(hr)) {
			throw std::runtime_error("D3D9 device lost!");
			return;
		}

		// chroma clear
		D3DCOLOR clearColor = D3DCOLOR_ARGB(0, 0, 255, 255);
		m_pD3D9Device->Clear(0, NULL, D3DCLEAR_TARGET, clearColor, 1.0f, 0);

		if (SUCCEEDED(m_pD3D9Device->BeginScene())) {
			DEVICE_CALL(m_pD3D9Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
			DEVICE_CALL(m_pD3D9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
			DEVICE_CALL(m_pD3D9Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));

			FLOAT fOffsetX = 0.0f;
			FLOAT fOffsetY = 0.0f;
			for (auto kv : m_WindowTextureMap) {
				if (kv.second == NULL) continue;

				D3DXMATRIX matProj;
				D3DXMatrixPerspectiveFovLH(&matProj, D3DXToRadian(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
				DEVICE_CALL(m_pD3D9Device->SetTransform(D3DTS_PROJECTION, &matProj));

				D3DXMATRIX matView;
				D3DXVECTOR3 origin(0.0f, 0.0f, -5.0f);
				D3DXVECTOR3 target(0.0f, 0.0f, 0.0f);
				D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
				D3DXMatrixLookAtLH(&matView,
					&origin,
					&target,
					&up);
				DEVICE_CALL(m_pD3D9Device->SetTransform(D3DTS_VIEW, &matView));

				D3DXMATRIX matRotY, matTranslate, matWorld;
				D3DXMatrixRotationY(&matRotY, D3DXToRadian(-60.0f));
				D3DXMatrixTranslation(&matTranslate, -0.5f + fOffsetX, 0.0f + fOffsetY, 2.0f);
				matWorld = matRotY * matTranslate;
				DEVICE_CALL(m_pD3D9Device->SetTransform(D3DTS_WORLD, &matWorld));

				DEVICE_CALL(m_pD3D9Device->SetTexture(0, kv.second));
				DEVICE_CALL(m_pD3D9Device->SetFVF(D3DFVF_VERTEX3D));

				DEVICE_CALL(m_pD3D9Device->SetRenderState(D3DRS_LIGHTING, FALSE));

				DEVICE_CALL(m_pD3D9Device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, g_QuadVertices, sizeof(SVertex3D)));

				fOffsetX += 0.05f;
				fOffsetY += 0.05f;
			}

			DEVICE_CALL(m_pD3D9Device->EndScene());
		}

		DEVICE_CALL(m_pD3D9Device->Present(NULL, NULL, NULL, NULL));
	}

	void CRendererApi::InitD3D9() {
		if (!SUCCEEDED(Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3D9))) {
			throw std::runtime_error("Failed to init Direct3D9!");
		}
	}

	void CRendererApi::CreateD3D9Device(UINT uAdapter, D3DDEVTYPE devType, HWND hWnd, UINT uMultiSampleLevel) {
		HRESULT hr = S_OK;
		DWORD dwFlags = devType == D3DDEVTYPE_HAL ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;

		ZeroMemory(&m_D3D9PresentParams, sizeof(D3DPRESENT_PARAMETERS));
		m_D3D9PresentParams.hDeviceWindow = hWnd;
		m_D3D9PresentParams.Windowed = TRUE;
		m_D3D9PresentParams.BackBufferFormat = D3DFMT_A8R8G8B8;
		m_D3D9PresentParams.MultiSampleType = (D3DMULTISAMPLE_TYPE)uMultiSampleLevel;
		m_D3D9PresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
		m_D3D9PresentParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

		D3DDISPLAYMODEEX mode;
		mode.Size = sizeof(D3DDISPLAYMODEEX);
		m_pD3D9->GetAdapterDisplayModeEx(uAdapter, &mode, NULL);

		hr = m_pD3D9->CheckDeviceFormat(uAdapter, devType,
			mode.Format, D3DUSAGE_RENDERTARGET,
			D3DRTYPE_SURFACE, m_D3D9PresentParams.BackBufferFormat);

		if (FAILED(hr)) {
			throw std::runtime_error("A8R8G8B8 is completely unsupported on this adapter setup.");
		}

		hr = m_pD3D9->CheckDeviceFormat(uAdapter, devType,
			mode.Format, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
			D3DRTYPE_TEXTURE, m_D3D9PresentParams.BackBufferFormat);
		static HRESULT reference = D3DERR_INVALIDCALL;
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to query alpha-capable backbuffer!");
		}

		hr = m_pD3D9->CreateDeviceEx(uAdapter, devType, hWnd, dwFlags,
			&m_D3D9PresentParams,
			NULL,
			&m_pD3D9Device);

		if (FAILED(hr)) {
			throw std::runtime_error("Failed to create D3D9 Device!");
		}
	}

	UINT CRendererApi::USelectD3D9HardwareAdapter(D3DDEVTYPE* pOutDevType) {
		OutputDebugString(L"Selecting D3D9 Hardware Accelerated Adapter\n");
		UINT uNumAdapters = m_pD3D9->GetAdapterCount();
		UINT uSelectedAdapter = D3DADAPTER_DEFAULT;
		BOOL bFoundHardware = FALSE;

		for (UINT uAdapter = 0; uAdapter < uNumAdapters; ++uAdapter) {
			D3DCAPS9 caps;
			HRESULT hr = m_pD3D9->GetDeviceCaps(uAdapter, D3DDEVTYPE_HAL, &caps);

			if (SUCCEEDED(hr)) {
				uSelectedAdapter = uAdapter;
				bFoundHardware = true;
				OutputDebugString(L"Found D3D9 Hardware Accelerated Adapter\n");
				break;
			}
		}

		if (!bFoundHardware) {
			OutputDebugString(L"No hardware adapter found. Falling back to software/REF.\n");
			return USelectD3D9SoftwareAdapter(pOutDevType);
		}
		*pOutDevType = D3DDEVTYPE_HAL;
		return uSelectedAdapter;
	}

	UINT CRendererApi::USelectD3D9SoftwareAdapter(D3DDEVTYPE* pOutDevType) {
		UINT uNumAdapters = m_pD3D9->GetAdapterCount();

		for (UINT uAdapter = 0; uAdapter < uNumAdapters; ++uAdapter) {
			D3DCAPS9 caps;
			HRESULT hr = m_pD3D9->GetDeviceCaps(uAdapter, D3DDEVTYPE_REF, &caps);

			if (SUCCEEDED(hr)) {
				OutputDebugString(L"Successfully selected D3D9 Software (Reference) Adapter.\n");
				*pOutDevType = D3DDEVTYPE_REF;
				return uAdapter;
			}
		}

		throw std::runtime_error("Failed to find an adapter supporting the Reference Rasterizer!");
	}

	void CRendererApi::ResetD3D9Device() {
		if (FAILED(m_pD3D9Device->Reset(&m_D3D9PresentParams))) {
			throw std::runtime_error("Failed to reset D3D9 device!");
		}
	}
}