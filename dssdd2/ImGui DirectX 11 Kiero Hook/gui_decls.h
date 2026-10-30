#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

// Renderer globals â€” defined in ui.cpp
extern ID3D11Device*            g_pd3dDevice;
extern ID3D11DeviceContext*     g_pd3dDeviceContext;
extern IDXGISwapChain*          g_pSwapChain;
extern ID3D11RenderTargetView*  g_mainRenderTargetView;

// Host-facing state â€” defined in ui.cpp
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
