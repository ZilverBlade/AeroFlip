#include "stdafx.h"
#include "CRendererApi.h"

#include <stdexcept>

namespace aeroflip {
#define DEVICE_CALL(call)															\
	do {																			\
		if (FAILED(call))															\
			throw std::runtime_error("Failed to perform device call '"#call"'!");	\
					} while (0)

	CRendererApi::CRendererApi(const SRendererApiConfig* pInConfig) : m_hWindow(pInConfig->hWnd) {
		InitD3D9();
		UINT uAdapter;
		D3DDEVTYPE devType;
		if (pInConfig->bHardwareAcceleration) {
			uAdapter = USelectD3D9HardwareAdapter(&devType);
		}
		else {
			uAdapter = USelectD3D9SoftwareAdapter(&devType);
		}

		CreateD3D9Device(uAdapter, devType, pInConfig->hWnd, pInConfig->uMultiSampleLevel);
	}
	CRendererApi::~CRendererApi() {
		SafeRelease(m_pD3D9Device);
		SafeRelease(m_pD3D9);
	}

	void CRendererApi::OnRender() {
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

		D3DCOLOR clearColor = D3DCOLOR_ARGB(0, 0, 0, 0);
		m_pD3D9Device->Clear(0, NULL, D3DCLEAR_TARGET, clearColor, 1.0f, 0);

		if (SUCCEEDED(m_pD3D9Device->BeginScene())) {
			DEVICE_CALL(m_pD3D9Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
			DEVICE_CALL(m_pD3D9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA));
			DEVICE_CALL(m_pD3D9Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA));

			D3DRECT testRect = { 50, 50, 250, 250 };
			DEVICE_CALL(m_pD3D9Device->Clear(1, &testRect, D3DCLEAR_TARGET, D3DCOLOR_XRGB(255, 0, 0), 1.0f, 0));

			DEVICE_CALL(m_pD3D9Device->EndScene());
		}

		DEVICE_CALL(m_pD3D9Device->Present(NULL, NULL, NULL, NULL));
	}

	void CRendererApi::InitD3D9() {
		if (!SUCCEEDED(Direct3DCreate9Ex(D3D_SDK_VERSION, &m_pD3D9))) {
			throw std::runtime_error("Failed to init Direct3D9!");
		}
	}

	void CRendererApi::CreateD3D9Device(UINT uInAdapter, D3DDEVTYPE inDevType, HWND hInWnd, UINT uInMultiSampleLevel) {
		HRESULT hr = S_OK;
		DWORD dwFlags = inDevType == D3DDEVTYPE_HAL ? D3DCREATE_HARDWARE_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING;

		ZeroMemory(&m_D3D9PresentParams, sizeof(D3DPRESENT_PARAMETERS));
		m_D3D9PresentParams.hDeviceWindow = hInWnd;
		m_D3D9PresentParams.Windowed = TRUE;
		m_D3D9PresentParams.BackBufferFormat = D3DFMT_A8R8G8B8;
		m_D3D9PresentParams.MultiSampleType = (D3DMULTISAMPLE_TYPE)uInMultiSampleLevel;
		m_D3D9PresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
		m_D3D9PresentParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

		D3DDISPLAYMODEEX mode;
		mode.Size = sizeof(D3DDISPLAYMODEEX);
		m_pD3D9->GetAdapterDisplayModeEx(uInAdapter, &mode, NULL);

		hr = m_pD3D9->CheckDeviceFormat(uInAdapter, inDevType,
			mode.Format, D3DUSAGE_RENDERTARGET,
			D3DRTYPE_SURFACE, m_D3D9PresentParams.BackBufferFormat);

		if (FAILED(hr)) {
			throw std::runtime_error("A8R8G8B8 is completely unsupported on this adapter setup.");
		}

		hr = m_pD3D9->CheckDeviceFormat(uInAdapter, inDevType,
			mode.Format, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING,
			D3DRTYPE_TEXTURE, m_D3D9PresentParams.BackBufferFormat);
		static HRESULT reference = D3DERR_INVALIDCALL;
		if (FAILED(hr))
		{
			throw std::runtime_error("Failed to query alpha-capable backbuffer!");
		}

		hr = m_pD3D9->CreateDeviceEx(uInAdapter, inDevType, hInWnd, dwFlags,
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