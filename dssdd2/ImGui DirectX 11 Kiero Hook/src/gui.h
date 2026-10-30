#pragma once
#include <windows.h>

// ── Renderer globals (defined in preview_main.cpp, shared with hook TU) ──
extern ID3D11Device*            g_pd3dDevice;
extern ID3D11DeviceContext*     g_pd3dDeviceContext;
extern IDXGISwapChain*          g_pSwapChain;
extern ID3D11RenderTargetView*  g_mainRenderTargetView;

// ── Host state ──
extern HWND  g_hwnd;          // preview window (preview build) / game window (injected)
extern bool  g_injected;      // true when running inside the game process
extern bool  guiVisible;
extern bool  g_dragActive;
extern float g_uiOffX;
extern float g_uiOffY;

// ── UI lifecycle (implemented in preview_main.cpp) ──
void GuiInit(HWND hWnd);      // imgui context, style, fonts, textures
void GuiRender();             // one frame: NewFrame → widgets → render into current RTV (no Present)
void GuiShutdown();           // imgui shutdown only
