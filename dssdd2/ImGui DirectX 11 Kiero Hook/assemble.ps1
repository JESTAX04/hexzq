# Phase 0 splice: preview_main.cpp (mock monolith) -> ui.cpp (host-agnostic) + preview_main.cpp (preview host)
$ErrorActionPreference = "Stop"
$root = "C:\Users\HEWS\Desktop\newzw\dssdd2\ImGui DirectX 11 Kiero Hook"
Set-Location $root
New-Item -ItemType Directory -Force -Path "src" | Out-Null
New-Item -ItemType Directory -Force -Path "loader" | Out-Null

$L = Get-Content -LiteralPath "preview_main.cpp" -Encoding UTF8

function Find-Line([scriptblock]$pred, [int]$from = 0) {
    for ($i = $from; $i -lt $L.Count; $i++) { if (&$pred $L[$i]) { return $i } }
    throw "anchor not found (from $from)"
}

$winMain   = Find-Line { $args[0] -match '^int WINAPI WinMain\(' }
$setupCtx  = Find-Line { $args[0] -match '// Setup Dear ImGui context' } $winMain
$initEnd   = Find-Line { $args[0] -match 'editor\.SetText\(""\);' } $setupCtx
$animStart = Find-Line { $args[0] -match '// Update GIF animation' } $initEnd
$animEnd   = Find-Line { $args[0].Trim() -eq '}' } ($animStart + 1)   # outer close brace of the if-block
$gifBlock  = ($L[$animStart..($animEnd)] -join "`r`n")
$newFrame  = Find-Line { $args[0].Trim() -eq 'ImGui_ImplDX11_NewFrame();' } $animEnd
$bodyStart = Find-Line { $args[0].Trim() -eq 'if (guiVisible)' } $newFrame
$renderLn  = Find-Line { $args[0].Trim() -eq 'ImGui::Render();' } $bodyStart

Write-Host "winMain=$winMain setup=$setupCtx initEnd=$initEnd gif=$animStart body=$bodyStart render=$renderLn total=$($L.Count)"

# ---------- head (before WinMain) ----------
$head = $L[0..($winMain-1)]
$head = $head -replace '^static ID3D11Device\*',            'ID3D11Device*'
$head = $head -replace '^static ID3D11DeviceContext\*',     'ID3D11DeviceContext*'
$head = $head -replace '^static IDXGISwapChain\*',          'IDXGISwapChain*'
$head = $head -replace '^static ID3D11RenderTargetView\*',  'ID3D11RenderTargetView*'
$head = $head -replace '^bool CreateDeviceD3D\(HWND hWnd\);', ''
$head = $head -replace '^void CleanupDeviceD3D\(\);', ''
$head = $head -replace '^void CreateRenderTarget\(\);', ''
$head = $head -replace '^void CleanupRenderTarget\(\);', ''
$head = $head -replace '^LRESULT WINAPI WndProc\(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam\);', ''
$head = $head | ForEach-Object {
    $_
    if ($_ -match '#include "TextEditor.hpp"') {
        '#include <commdlg.h>'
        '#include "src/engine.h"'
        '#pragma comment(lib, "comdlg32.lib")'
        ''
        '// ── Host-facing state ──'
        'HWND  g_hwnd = nullptr;      // set by GuiInit'
        'bool  g_injected = false;    // true when running inside the game'
        'bool  g_dragActive = false;'
        'bool  g_requestExit = false;'
        'float g_uiOffX = 0.0f;'
        'float g_uiOffY = 0.0f;'
    }
}
$head = $head -join "`r`n"

# ---------- GuiInit ----------
$init = $L[($setupCtx)..($initEnd)] -join "`r`n"

# ---------- GuiRender body ----------
$body = $L[$bodyStart..($renderLn-1)] -join "`r`n"
$body = $body -replace '(?m)^\s*SendMessage\(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0\);\r?$', '					g_dragActive = true;'
$body = $body -replace 'PostQuitMessage\(0\);', 'g_requestExit = true;'
$body = $body -replace 'ShowWindow\(hWnd, SW_MINIMIZE\);', 'guiVisible = false;'
$body = $body -replace 'ofn\.hwndOwner = hWnd;', 'ofn.hwndOwner = g_hwnd;'
$body = $body -replace 'float windowStartX = \(io\.DisplaySize\.x - windowWidth\) \* 0\.5f;', 'float windowStartX = (io.DisplaySize.x - windowWidth) * 0.5f + g_uiOffX;'
$body = $body -replace 'float windowStartY = \(io\.DisplaySize\.y - windowHeight\) \* 0\.5f;', 'float windowStartY = (io.DisplaySize.y - windowHeight) * 0.5f + g_uiOffY;'
$hWndLeft = ([regex]::Matches($body, '\bhWnd\b')).Count
Write-Host "hWnd refs remaining in body: $hWndLeft (expect 0)"

$guiRender = @"
void GuiRender()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();

	// ── Manual overlay drag (host-agnostic: never touches the host window) ──
	if (g_dragActive)
	{
		if (!io.MouseDown[0])
			g_dragActive = false;
		g_uiOffX += io.MouseDelta.x;
		g_uiOffY += io.MouseDelta.y;
	}

	// GIF animation
$gifBlock

$body

	ImGui::Render();
}

void GuiShutdown()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}
"@

$guiInit = @"
void GuiInit(HWND hWnd)
{
	g_hwnd = hWnd;
$init
}
"@

$uiCpp = @"
// ui.cpp — host-agnostic UI + render pipeline. Assembled from the old preview monolith.
// Contract:
//   GuiInit(hWnd)   -> ImGui context, style, fonts, textures (once, render thread, after device exists)
//   GuiRender()     -> NewFrame -> widgets -> ImGui::Render (every frame)
//   [host]          -> OMSetRenderTargets / Clear / ImGui_ImplDX11_RenderDrawData / Present
//   GuiShutdown()   -> teardown

#include "gui_decls.h"

$head

$guiInit

$guiRender
"@

Set-Content -LiteralPath "ui.cpp" -Value $uiCpp -Encoding UTF8

# ---------- gui_decls.h ----------
$decls = @"
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

// Renderer globals — defined in ui.cpp
extern ID3D11Device*            g_pd3dDevice;
extern ID3D11DeviceContext*     g_pd3dDeviceContext;
extern IDXGISwapChain*          g_pSwapChain;
extern ID3D11RenderTargetView*  g_mainRenderTargetView;

// Host-facing state — defined in ui.cpp
extern HWND  g_hwnd;
extern bool  g_injected;
extern bool  guiVisible;
extern bool  g_dragActive;
extern bool  g_requestExit;
extern float g_uiOffX;
extern float g_uiOffY;

void GuiInit(HWND hWnd);
void GuiRender();
void GuiShutdown();
"@
Set-Content -LiteralPath "gui_decls.h" -Value $decls -Encoding UTF8

# ---------- new preview host ----------
$previewHost = @"
// preview_main.cpp — standalone preview host (own window + device).
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
			PostQuitMessage(0);
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
"@
if (-not (Test-Path "preview_mock_backup.cpp.bak")) { Copy-Item "preview_main.cpp" "preview_mock_backup.cpp.bak" }
Set-Content -LiteralPath "preview_main.cpp" -Value $previewHost -Encoding UTF8

Write-Host "OK: ui.cpp=$((Get-Content ui.cpp).Count) lines, preview_main.cpp=$((Get-Content preview_main.cpp).Count) lines"
