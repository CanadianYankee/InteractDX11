#pragma once

#include "WindowCluster.h"
#include "DrawTimer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#if (defined(DEBUG) || defined(_DEBUG))
#define D3DDEBUGNAME(pobj, name) pobj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name)
#else
#define D3DDEBUGNAME(pobj, name)
#endif 

class CSaverBase
{
public:
	CSaverBase(void);
	virtual ~CSaverBase(void);

	BOOL Initialize(const CWindowCluster::SAVER_WINDOW &myWindow, int iSaverIndex, int iNumSavers);
	bool Tick();	// Returns true if a frame was simulated and presented
	bool IsActive() const { return m_bRunning && !m_bOccluded; }
	void Pause();
	void Resume();
	void ResumeResized(int cx, int cy);
	void SetSyncInterval(UINT uSyncInterval) { m_uSyncInterval = uSyncInterval; }
	void CleanUp();

protected:
	// Override these functions to implement your saver - return FALSE to error out and shut down
	virtual BOOL InitSaverData() = 0;
	virtual BOOL IterateSaver(float dt, float T) = 0;
	virtual BOOL PauseSaver() = 0;
	virtual BOOL ResumeSaver() = 0;
	virtual BOOL OnResizeSaver() = 0;
	virtual void CleanUpSaver() = 0;

	// Utility functions for saver class to use
	enum ShaderType 
	{	VertexShader, PixelShader, GeometryShader, ComputeShader, HullShader, DomainShader };
	struct VS_INPUTLAYOUTSETUP
	{
		const D3D11_INPUT_ELEMENT_DESC *pInputDesc;
		UINT NumElements;
		ID3D11InputLayout *pInputLayout;
	};
	HRESULT LoadShader(ShaderType type, const std::wstring &strFileName, ComPtr<ID3D11ClassLinkage> pClassLinkage, ComPtr<ID3D11DeviceChild> *pShader, VS_INPUTLAYOUTSETUP *pILS = NULL);
	HRESULT LoadTexture(const std::wstring &strFileName, ComPtr<ID3D11Texture2D> *ppTexture, ComPtr<ID3D11ShaderResourceView> *ppSRVTexture);
	// Present the back buffer; handles DXGI_STATUS_OCCLUDED. Returns FALSE on a real failure.
	BOOL PresentFrame();

private:
	// Internal utility functions
	HRESULT InitDirect3D();
	HRESULT GetMyAdapter(IDXGIAdapter **ppAdapter); 
	HRESULT LoadBinaryFile(const std::wstring &strPath, std::vector<char> &buffer);
	BOOL OnResize();

protected:
	ComPtr<ID3D11Device> m_pD3DDevice;
	ComPtr<ID3D11DeviceContext> m_pD3DContext;
	ComPtr<IDXGISwapChain> m_pSwapChain;
	ComPtr<ID3D11Texture2D> m_pDepthStencilBuffer;
	ComPtr<ID3D11RenderTargetView> m_pRenderTargetView;
	ComPtr<ID3D11DepthStencilView> m_pDepthStencilView;
	D3D11_VIEWPORT m_ScreenViewport;

	UINT m_4xMsaaQuality;
	bool m_bEnable4xMsaa;

	std::wstring m_strShaderPath;
	std::wstring m_strGFXResoucePath;

	HWND m_hMyWindow;
	int m_iSaverIndex;
	int m_iNumSavers;
	int m_iClientWidth;
	int m_iClientHeight;
	bool m_bFullScreen;
	float m_fAspectRatio;
	bool m_bRunning;
	CDrawTimer m_Timer;
	UINT m_uSyncInterval = 1;	// Present() sync interval; see CWindowCluster::Run

	// Set false in a derived constructor if the saver never depth-tests (saves a full-screen
	// buffer and its per-frame clear).
	bool m_bUseDepthStencil = true;

	// When nothing we present is visible (locked session, display off, window fully covered or
	// minimized) Present returns DXGI_STATUS_OCCLUDED without waiting for vsync. We then stop
	// simulating and only re-test visibility every OCCLUSION_POLL_MS.
	static const ULONGLONG OCCLUSION_POLL_MS = 250;
	bool m_bOccluded = false;
	ULONGLONG m_ullNextOcclusionTest = 0;
};

