#pragma once

#include "saverbase.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class CCube :
	public CSaverBase
{
public:
	CCube(void);
	virtual ~CCube(void);

protected:
	virtual BOOL InitSaverData();
	virtual BOOL IterateSaver(SAVERFLOAT dt, SAVERFLOAT T);
	virtual BOOL PauseSaver();
	virtual BOOL ResumeSaver();
	virtual BOOL OnResize();
	virtual void CleanUpSaver();

protected:
	virtual BOOL UpdateScene(float dt, float T);
	virtual BOOL RenderScene();

	float m_clrBackground[4];

	HRESULT InitDirect3D();
	HRESULT GetMyAdapter(IDXGIAdapter **ppAdapter); 

	ComPtr<ID3D11Device> m_pD3DDevice;
	ComPtr<ID3D11DeviceContext> m_pD3DContext;
	ComPtr<IDXGISwapChain> m_pSwapChain;
	ComPtr<ID3D11Texture2D> m_pDepthStencilBuffer;
	ComPtr<ID3D11RenderTargetView> m_pRenderTargetView;
	ComPtr<ID3D11DepthStencilView> m_pDepthStencilView;
	D3D11_VIEWPORT m_ScreenViewport;
	UINT m_4xMsaaQuality;
	bool m_bEnable4xMsaa;

	float m_fD2DHalfWidth;
	float m_fD2DHalfHeight;
};

