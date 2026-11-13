#include "stdafx.h"
#include "Resource.h"
#include "WindowCluster.h"
#include "SaverEngine.h"
#include "SaverBase.h"
#include "InteractSaver.h"
#include "ConfigDialog.h"

#include <wtsapi32.h>
#pragma comment(lib, "wtsapi32.lib")

#define MAX_LOADSTRING 100

CWindowCluster *CWindowCluster::s_pTheCluster = nullptr;

LRESULT CALLBACK GlobalWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	assert(CWindowCluster::s_pTheCluster);
	return CWindowCluster::s_pTheCluster->ClusterWndProc(hwnd, msg, wParam, lParam);
}

CWindowCluster::CWindowCluster(CSaverEngine *pEngine) :
	m_pParentEngine(pEngine),
	m_blackBrush(NULL),
	m_bResizing(false),
	// In full-screen saver mode, run an independent saver on every monitor. Windowed and
	// preview modes only ever have a single window.
	m_bMultiRender(pEngine->GetMode() == CSaverEngine::sdmSaverWindow)
{
	assert(!s_pTheCluster);
	s_pTheCluster = this;
	m_ptMouse.x = m_ptMouse.y = -1;
}

CWindowCluster::~CWindowCluster(void)
{
	s_pTheCluster = nullptr;
}

BOOL CWindowCluster::AddPrimary(LPRECT lpRect, BOOL bSystemPrimary)
{
	assert(s_pTheCluster == this);
	
	SAVER_WINDOW windowInfo;
	ZeroMemory(&windowInfo, sizeof(windowInfo));

	windowInfo.bSaverMode = m_pParentEngine->GetMode() == CSaverEngine::sdmSaverWindow;
	windowInfo.bNested = false;
	windowInfo.bPrimary = true;
	windowInfo.rcCreateArea = *lpRect;
	if(windowInfo.bSaverMode)
	{
		windowInfo.sClientArea.cx = windowInfo.rcCreateArea.right - windowInfo.rcCreateArea.left;
		windowInfo.sClientArea.cy = windowInfo.rcCreateArea.bottom - windowInfo.rcCreateArea.top;
		windowInfo.dwCreateFlags = WS_POPUP;
	}
	else
	{
		// Desired width is trimmed around by 1/8 screen for normal window, down to 1/8 screen for floating preview
		int w = windowInfo.rcCreateArea.right - windowInfo.rcCreateArea.left;
		int h = windowInfo.rcCreateArea.bottom - windowInfo.rcCreateArea.top;
		if(m_pParentEngine->GetMode() == CSaverEngine::sdmPreviewWindow)
		{
			w = w >> 3; h = h >> 3;
		}
		else
		{
			w = (w >> 2) * 3; h = (h >> 2) * 3;
		}
		int cx = (windowInfo.rcCreateArea.right + windowInfo.rcCreateArea.left) >> 1;
		int cy = (windowInfo.rcCreateArea.top + windowInfo.rcCreateArea.bottom) >> 1;
		windowInfo.rcCreateArea.left	= cx - (w >> 1); 
		windowInfo.rcCreateArea.right	= cx + (w >> 1);
		windowInfo.rcCreateArea.top		= cy - (h >> 1); 
		windowInfo.rcCreateArea.bottom	= cy + (h >> 1);

		windowInfo.dwCreateFlags = WS_OVERLAPPEDWINDOW;
		windowInfo.sClientArea.cx = windowInfo.rcCreateArea.right - windowInfo.rcCreateArea.left;
		windowInfo.sClientArea.cy = windowInfo.rcCreateArea.bottom - windowInfo.rcCreateArea.top;
		::AdjustWindowRect(&windowInfo.rcCreateArea, windowInfo.dwCreateFlags, TRUE);
	}

	// Always put the primary window at the beginning of the vector
	if(bSystemPrimary)
		m_vecWindows.insert(m_vecWindows.begin(), windowInfo);
	else
		m_vecWindows.push_back(windowInfo);

	return TRUE;
}

BOOL CWindowCluster::AddSecondary(LPRECT lpRect)
{
	assert(s_pTheCluster == this);
	
	if(m_pParentEngine->GetMode() != CSaverEngine::sdmSaverWindow)
		return FALSE;

	SAVER_WINDOW windowInfo;
	ZeroMemory(&windowInfo, sizeof(windowInfo));

	windowInfo.bSaverMode = true;
	windowInfo.bNested = false;
	windowInfo.bPrimary = false;
	windowInfo.rcCreateArea = *lpRect;
	windowInfo.dwCreateFlags = WS_POPUP;

	m_vecWindows.push_back(windowInfo);

	return TRUE;
}

BOOL CWindowCluster::AddNested(HWND hParentWnd)
{
	assert(s_pTheCluster == this);
	assert(m_vecWindows.size() == 0);
	
	if(m_pParentEngine->GetMode() != CSaverEngine::sdmPreviewWindow)
		return FALSE;


	SAVER_WINDOW windowInfo;
	ZeroMemory(&windowInfo, sizeof(windowInfo));

	BOOL bSuccess = GetClientRect(hParentWnd, &windowInfo.rcCreateArea);
	if(!bSuccess)
		return FALSE;
	windowInfo.bSaverMode = false;
	windowInfo.bNested = true;
	windowInfo.bPrimary = true;
	windowInfo.hParentWnd = hParentWnd;
	windowInfo.sClientArea.cx = windowInfo.rcCreateArea.right - windowInfo.rcCreateArea.left;
	windowInfo.sClientArea.cy = windowInfo.rcCreateArea.bottom - windowInfo.rcCreateArea.top;
	windowInfo.dwCreateFlags = WS_CHILD;

	m_vecWindows.push_back(windowInfo);

	return TRUE;

}

BOOL CWindowCluster::CreateAll()
{
	// There should be at least one window, at least the first of which must be "primary"
	if(m_vecWindows.size() == 0)
		return FALSE;

	if(!m_vecWindows.begin()->bPrimary)
		return FALSE;

	std::wstring strTitle, strWndClass;
	TCHAR szBuffer[MAX_LOADSTRING];
	LoadString(m_pParentEngine->GetAppInstance(), IDS_APP_TITLE, szBuffer, MAX_LOADSTRING);
	strTitle = szBuffer;
	LoadString(m_pParentEngine->GetAppInstance(), IDC_INTERACTDX11, szBuffer, MAX_LOADSTRING);
	strWndClass = szBuffer;

	bool bRegPrimary = false;
	bool bRegSecondary = false;
	BOOL bSuccess = true;

	int numSavers = m_bMultiRender ? static_cast<int>(m_vecWindows.size()) : 1;
	int iSaverIndex = 0;
	for(WindowIter iWindow = m_vecWindows.begin(); iWindow != m_vecWindows.end() && bSuccess; ++iWindow)
	{
		if(iWindow->bPrimary && !bRegPrimary)
		{
			RegisterWindowClass(*iWindow, strWndClass);
			bRegPrimary = true;
		}
		else if(!iWindow->bPrimary && !bRegSecondary)
		{
			RegisterWindowClass(*iWindow, strWndClass + L"2");
			bRegSecondary = true;
		}
		iWindow->hWnd = CreateWindow(strWndClass.c_str(), strTitle.c_str(), iWindow->dwCreateFlags, iWindow->rcCreateArea.left, iWindow->rcCreateArea.top, 
			iWindow->rcCreateArea.right - iWindow->rcCreateArea.left, iWindow->rcCreateArea.bottom - iWindow->rcCreateArea.top, 
			iWindow->hParentWnd, NULL, m_pParentEngine->GetAppInstance(), 0);

		bSuccess = bSuccess && (iWindow->hWnd != NULL);

		// Create saver
		if(iWindow->bPrimary)
		{
			iWindow->pSaver = new CInteractSaver;
			bSuccess = bSuccess && (iWindow->pSaver->Initialize(*iWindow, iSaverIndex++, numSavers));
		}
	}

	assert(!bSuccess || iSaverIndex == numSavers);


	return bSuccess;
}

void CWindowCluster::RegisterWindowClass(SAVER_WINDOW windowInfo, const std::wstring &strClass)
{
	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(wc));

	if(m_blackBrush == NULL)
	{
		m_blackBrush = CreateSolidBrush(RGB(0,0,0));
	}

	UINT uStyle = CS_HREDRAW | CS_VREDRAW;
	if(windowInfo.bPrimary)
		uStyle |= CS_OWNDC;

	wc.cbSize = sizeof(wc);
	wc.style			= uStyle;
	wc.lpfnWndProc		= GlobalWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= m_pParentEngine->GetAppInstance();
	wc.hIcon			= LoadIcon(m_pParentEngine->GetAppInstance(), MAKEINTRESOURCE(IDI_INTERACTDX11));
	wc.hCursor			= windowInfo.bSaverMode ? LoadCursor(m_pParentEngine->GetAppInstance(), MAKEINTRESOURCE(IDC_NULL)) : LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground	= m_blackBrush;
	wc.lpszMenuName		= (windowInfo.bSaverMode || windowInfo.bNested) ? NULL : MAKEINTRESOURCE(IDC_INTERACTDX11);
	wc.lpszClassName	= strClass.c_str();
	wc.hIconSm			= LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	RegisterClassEx(&wc);
}

CWindowCluster::WindowIter CWindowCluster::LookupWindow(HWND hWnd)
{
	WindowIter iter = m_vecWindows.begin();
	for(; iter != m_vecWindows.end(); ++iter)
	{
		if(iter->hWnd == hWnd)
			break;
	}

	return iter;
}

void CWindowCluster::Run()
{
	MSG msg = {};
	HACCEL hAccelTable;

	for(WindowIter iWindow = m_vecWindows.begin(); iWindow != m_vecWindows.end(); ++iWindow)
	{
		ShowWindow(iWindow->hWnd, SW_SHOW);
		UpdateWindow(iWindow->hWnd);
		BringWindowToTop(iWindow->hWnd);
	}

	SetActiveWindow(m_vecWindows.front().hWnd);

	// Get told when the displays power off/on so we can stop rendering to dark screens.
	// Windows sends the current state immediately after registration.
	m_hPowerNotify = RegisterPowerSettingNotification(m_vecWindows.front().hWnd,
		&GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_WINDOW_HANDLE);

	// Get told when the session locks/unlocks. On the lock screen DXGI reports our window as
	// occluded only briefly, then lets us keep presenting, so occlusion alone isn't enough.
	m_bSessionNotify = WTSRegisterSessionNotification(m_vecWindows.front().hWnd, NOTIFY_FOR_THIS_SESSION) != FALSE;

	hAccelTable = LoadAccelerators(m_pParentEngine->GetAppInstance(), MAKEINTRESOURCE(IDC_INTERACTDX11));

	// Main message loop:
	do
	{
		// If there are Window messages then process them.
		if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
            TranslateMessage( &msg );
            DispatchMessage( &msg );
		}
		// Otherwise, let the saver do its thing
		else
        {
			// Savers are ticked one after another. If every Present waited for vsync, N monitors
			// would run at 1/N of the refresh rate, so only the last saver that will actually
			// render this pass waits; it throttles the whole loop once per frame. Chosen per pass
			// because a saver drops out while paused or occluded.
			CSaverBase *pVSyncSaver = nullptr;
			for(WindowIter iWindow = m_vecWindows.begin(); iWindow != m_vecWindows.end(); ++iWindow)
			{
				if(iWindow->pSaver)
				{
					iWindow->pSaver->SetSyncInterval(0);
					if(iWindow->pSaver->IsActive())
						pVSyncSaver = iWindow->pSaver;
				}
			}
			if(pVSyncSaver)
				pVSyncSaver->SetSyncInterval(1);

			bool bRendered = false;
			for(WindowIter iWindow = m_vecWindows.begin(); iWindow != m_vecWindows.end(); ++iWindow)
			{
				// The preview host lives in another process (the Screen Saver Settings dialog).
				// If it goes away without destroying us cleanly, exit rather than linger invisibly.
				if(iWindow->bNested && !IsWindow(iWindow->hParentWnd))
				{
					PostQuitMessage(0);
					break;
				}

				if(iWindow->pSaver && iWindow->pSaver->Tick())
				{
					bRendered = true;
				}
			}

			if(!bRendered)
			{
				// Nothing to draw (paused, minimized, occluded, or displays off). Sleep until a
				// message arrives, waking periodically to re-test occlusion and the preview host.
				MsgWaitForMultipleObjectsEx(0, nullptr, IDLE_POLL_MS, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
			}
		}
	}  while(msg.message != WM_QUIT);
}

// dwState is from GUID_CONSOLE_DISPLAY_STATE: 0 = off, 1 = on, 2 = dimmed (still visible).
void CWindowCluster::OnDisplayStateChange(DWORD dwState)
{
	bool bWasHidden = IsHidden();
	m_bDisplaysOff = (dwState == 0);
	OnHiddenChanged(bWasHidden);
}

void CWindowCluster::OnSessionChange(WPARAM wParam)
{
	if(wParam != WTS_SESSION_LOCK && wParam != WTS_SESSION_UNLOCK)
		return;

	bool bWasHidden = IsHidden();
	m_bSessionLocked = (wParam == WTS_SESSION_LOCK);
	OnHiddenChanged(bWasHidden);
}

// Pause every saver when nobody can see them (displays off or session locked); resume when
// that ends, except for windows that are minimized or mid-drag (those resume on their own).
void CWindowCluster::OnHiddenChanged(bool bWasHidden)
{
	bool bHidden = IsHidden();
	if(bHidden == bWasHidden)
		return;

	for(WindowIter iWindow = m_vecWindows.begin(); iWindow != m_vecWindows.end(); ++iWindow)
	{
		if(!iWindow->pSaver)
			continue;

		if(bHidden)
			iWindow->pSaver->Pause();
		else if(!IsIconic(iWindow->hWnd) && !m_bResizing)
			iWindow->pSaver->Resume();
	}
}

void CWindowCluster::DestroyAll()
{
	if(m_bSessionNotify && !m_vecWindows.empty())
	{
		WTSUnRegisterSessionNotification(m_vecWindows.front().hWnd);
		m_bSessionNotify = false;
	}

	if(m_vecWindows.size() > 0)
	{
		for(WindowIter iWindow = m_vecWindows.begin(); iWindow != m_vecWindows.end(); ++iWindow)
		{ 
			if(iWindow->pSaver)
			{
				iWindow->pSaver->CleanUp();
				delete iWindow->pSaver;
				iWindow->pSaver = NULL;
			}
			DestroyWindow(iWindow->hWnd);
			iWindow->hWnd = NULL;
		}

		m_vecWindows.clear();
	}

	if(m_hPowerNotify)
	{
		UnregisterPowerSettingNotification(m_hPowerNotify);
		m_hPowerNotify = NULL;
	}

	if(m_blackBrush)
	{
		DeleteObject(m_blackBrush);
		m_blackBrush = NULL;
	}
}

LRESULT CWindowCluster::ClusterWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Figure out which window has been sent the message
	WindowIter iWindow = LookupWindow(hWnd);
	if(iWindow == m_vecWindows.end())
	{
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	LRESULT lRet = 0;
	bool bHandled = false;

	if(msg == WM_WTSSESSION_CHANGE)
	{
		OnSessionChange(wParam);
		return 0;
	}

	if(msg == WM_POWERBROADCAST && wParam == PBT_POWERSETTINGCHANGE)
	{
		const POWERBROADCAST_SETTING *pSetting = reinterpret_cast<const POWERBROADCAST_SETTING *>(lParam);
		if(pSetting && IsEqualGUID(pSetting->PowerSetting, GUID_CONSOLE_DISPLAY_STATE) &&
			pSetting->DataLength >= sizeof(DWORD))
		{
			OnDisplayStateChange(*reinterpret_cast<const DWORD *>(pSetting->Data));
		}
		return TRUE;
	}

	if(iWindow->bSaverMode)
	{
		bool bQuit = false;

		// Saver needs to be stopped in response to key presses or mouse events
		switch(msg)
		{
		case WM_MOUSEMOVE:
			{
				short xPos = GET_X_LPARAM(lParam); 
				short yPos = GET_Y_LPARAM(lParam); 
				if(m_ptMouse.x < 0)
				{
					m_ptMouse.x = xPos;
					m_ptMouse.y = yPos;
				}
				else
				{
					bQuit = (xPos != m_ptMouse.x) || (yPos != m_ptMouse.y);
				}
			}
			break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_MOUSEWHEEL:
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			bQuit = true;
			break;
		}

		if(bQuit)
		{
			PostMessage(m_vecWindows.front().hWnd, WM_DESTROY, 0, 0);
			bHandled = true;
		}
	}

	// Primary window processes commands
	if(iWindow->bPrimary && iWindow->pSaver && !bHandled)
	{
		switch(msg)
		{
			//case WM_ACTIVATE:
			//	if( LOWORD(wParam) == WA_INACTIVE )
			//	{
			//		iWindow->pSaver->Pause();
			//	}
			//	else
			//	{
			//		iWindow->pSaver->Resume();
			//	}
			//	bHandled = true;
			//	break;

			case WM_COMMAND:
			{
				WORD wmId    = LOWORD(wParam);
				WORD wmEvent = HIWORD(wParam);
				// Parse the menu selections:
				switch (wmId)
				{
				case IDM_CONFIG:
					if(iWindow->pSaver)
					{
						iWindow->pSaver->Pause();
						CConfigDialog dlg;
						dlg.ShowModal(m_pParentEngine->GetAppInstance(), iWindow->hWnd);
						iWindow->pSaver->Resume();
					}
					bHandled = true;
					break;

				case IDM_EXIT:
					PostMessage(iWindow->hWnd, WM_DESTROY, 0, 0);
					bHandled = true;
					break;
				}
				break;
			}
			break;

		case WM_SIZE:
			{
				// Save the new client area dimensions.
				iWindow->sClientArea.cx = LOWORD(lParam);
				iWindow->sClientArea.cy = HIWORD(lParam);

				if( wParam == SIZE_MINIMIZED )
				{
					iWindow->pSaver->Pause();
				}
				else if( wParam == SIZE_MAXIMIZED )
				{
					iWindow->pSaver->ResumeResized(iWindow->sClientArea.cx, iWindow->sClientArea.cy);
					if(IsHidden())
						iWindow->pSaver->Pause();	// Resized while locked or displays off; stay paused
				}
				else if(!m_bResizing)
				{
					iWindow->pSaver->ResumeResized(iWindow->sClientArea.cx, iWindow->sClientArea.cy);
					if(IsHidden())
						iWindow->pSaver->Pause();	// Resized while locked or displays off; stay paused
				}
				bHandled = true;
				break;
			}

		case WM_DPICHANGED:
			{
				// Windowed mode moved to a monitor with a different scale factor: accept the
				// suggested window rect. The resulting WM_SIZE resizes the swap chain.
				if(!iWindow->bSaverMode && !iWindow->bNested)
				{
					const RECT *prcNew = reinterpret_cast<const RECT *>(lParam);
					SetWindowPos(hWnd, NULL, prcNew->left, prcNew->top,
						prcNew->right - prcNew->left, prcNew->bottom - prcNew->top,
						SWP_NOZORDER | SWP_NOACTIVATE);
				}
				bHandled = true;
				break;
			}

		case WM_ENTERSIZEMOVE:
			{
				m_bResizing = true;
				iWindow->pSaver->Pause();
				bHandled = true;
				break;
			}

		case WM_EXITSIZEMOVE:
			{
				m_bResizing = false;
				RECT rcClient;	
				GetClientRect(iWindow->hWnd, &rcClient);
				iWindow->pSaver->ResumeResized(iWindow->sClientArea.cx, iWindow->sClientArea.cy);
				if(IsHidden())
					iWindow->pSaver->Pause();	// Resized while locked or displays off; stay paused
				bHandled = true;
				break;
			}

		case WM_PAINT:
			ValidateRect(iWindow->hWnd, NULL);
			lRet = TRUE;
			bHandled = true;
			break;	

		case WM_ERASEBKGND:
			lRet = TRUE;
			bHandled = true;
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			bHandled = true;
			break;
		}
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}
