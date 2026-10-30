// preview_main.cpp â€” standalone preview host (own window + device).
// The injected DLL host lives in src/dllmain.cpp and calls the same Gui* contract.
#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <dwmapi.h>
#include "gui_decls.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "src/engine.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, hInstance, nullptr, nullptr, nullptr, nullptr, _T("redEngineOverlayPreview"), nullptr };
	RegisterClassEx(&wc);

	int winWidth = GetSystemMetrics(SM_CXSCREEN);
	int winHeight = GetSystemMetrics(SM_CYSCREEN);

	HWND hWnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED,
		wc.lpszClassName, _T("redENGINE Overlay Preview"), WS_POPUP,
		0, 0, winWidth, winHeight,
		nullptr, nullptr, wc.hInstance, nullptr);

	SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 255, LWA_ALPHA);
	MARGINS margins = { -1, -1, -1, -1 };
	DwmExtendFrameIntoClientArea(hWnd, &margins);

	if (!CreateDeviceD3D(hWnd))
	{
		CleanupDeviceD3D();
		UnregisterClass(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);

	g_injected = false;
	red::InitEngine();
	GuiInit(hWnd);

	bool done = false;
	while (!done)
	{
		MSG msg;
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				done = true;
		}

		red::PumpGameThread();

		// Async hotkeys so the preview is always controllable:
		//   INSERT -> toggle UI   |   HOME -> close preview
		{
			static bool prevIns = false, prevHome = false;
			bool ins = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
			bool home = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
			if (ins && !prevIns) guiVisible = !guiVisible;
			if (home && !prevHome) break;
			prevIns = ins; prevHome = home;
		}

		if (g_requestExit)
			break;

		GuiRender();

		const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
		g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		g_pSwapChain->Present(1, 0);
	}

	GuiShutdown();
	CleanupDeviceD3D();
	DestroyWindow(hWnd);
	UnregisterClass(wc.lpszClassName, wc.hInstance);
	return 0;
}

bool CreateDeviceD3D(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL fl[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
	HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fl, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res == DXGI_ERROR_UNSUPPORTED)
		res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, fl, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res != S_OK)
		return false;

	CreateRenderTarget();
	return true;
}

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer = nullptr;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	if (pBackBuffer)
	{
		g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
		pBackBuffer->Release();
	}
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			// ESC is reserved for in-game menus in injected mode; don't kill the preview.
			return 0;
		}
		if (wParam == VK_INSERT)
		{
			guiVisible = !guiVisible;
			return 0;
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
