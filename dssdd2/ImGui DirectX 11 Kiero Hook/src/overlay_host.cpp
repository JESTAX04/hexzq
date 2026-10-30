// overlay_host.cpp — injected host: hooks the game's IDXGISwapChain::Present via VMT patch,
// drives the host-agnostic Gui* contract, subclasses the game WndProc for input.
#include "../gui_decls.h"
#include "../src/engine.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_impl_dx11.h"
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <mutex>
#include <cstddef>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "user32.lib")

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace ov {

// ── VMT hooks ──
typedef HRESULT(STDMETHODCALLTYPE* fnPresent)(IDXGISwapChain*, UINT, UINT);
typedef HRESULT(STDMETHODCALLTYPE* fnResizeBuffers)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

// The game may own a swapchain on a different internal DXGI class (legacy DISCARD
// vs FLIP model) than our temp chains — keep one record per vtable so each class
// calls back into its own original functions.
struct PatchedVmt {
	void**          vmt;
	fnPresent       origPresent;
	fnResizeBuffers origResize;
};
static std::vector<PatchedVmt> g_patched;
static std::mutex              g_vmtMtx;

static ID3D11Texture2D*         g_rtvTex = nullptr;
static WNDPROC                  g_origWndProc = nullptr;
static bool                     g_ready = false;
static bool                     g_resetPending = false;

static void OvLog(const char* msg)
{
	char path[MAX_PATH];
	GetTempPathA(MAX_PATH, path);
	strcat_s(path, "redengine_debug.log");
	FILE* f = nullptr;
	fopen_s(&f, path, "a");
	if (f) { fputs(msg, f); fputc('\n', f); fclose(f); }
}

bool IsArmed() { return !g_patched.empty(); }

static void CreateRTV(IDXGISwapChain* sc)
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
	ID3D11Texture2D* back = nullptr;
	if (SUCCEEDED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back)) && back)
	{
		g_pd3dDevice->CreateRenderTargetView(back, nullptr, &g_mainRenderTargetView);
		back->Release();
	}
}

extern HRESULT STDMETHODCALLTYPE Hk_Present(IDXGISwapChain*, UINT, UINT);
extern HRESULT STDMETHODCALLTYPE Hk_ResizeBuffers(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

bool HookSwapchain(IDXGISwapChain* sc)
{
	std::lock_guard<std::mutex> lk(g_vmtMtx);

	void** vmt = *reinterpret_cast<void***>(sc);

	// This vtable already patched?
	for (auto& p : g_patched)
		if (p.vmt == vmt) return true;

	DWORD old = 0;
	if (!VirtualProtect(vmt, sizeof(void*) * 32, PAGE_READWRITE, &old))
	{
		OvLog("[ov] VirtualProtect(vmt) FAILED");
		return false;
	}

	// Layout verified against Windows SDK dxgi.h with offsetof:
	//   Present = 8, GetBuffer = 9, GetDesc = 12, ResizeBuffers = 13
	PatchedVmt rec{};
	rec.vmt = vmt;
	rec.origPresent = reinterpret_cast<fnPresent>(vmt[8]);
	rec.origResize  = reinterpret_cast<fnResizeBuffers>(vmt[13]);

	char m[160];
	wsprintfA(m, "[ov] patching vmt=%p origPresent=%p", (void*)vmt, (void*)rec.origPresent);
	OvLog(m);

	vmt[8]  = reinterpret_cast<void*>(&Hk_Present);
	vmt[13] = reinterpret_cast<void*>(&Hk_ResizeBuffers);

	VirtualProtect(vmt, sizeof(void*) * 32, old, &old);
	FlushInstructionCache(GetCurrentProcess(), vmt, sizeof(void*) * 32);
	g_patched.push_back(rec);
	return true;
}

static fnPresent FindOrigPresent(IDXGISwapChain* sc)
{
	void** vmt = *reinterpret_cast<void***>(sc);
	std::lock_guard<std::mutex> lk(g_vmtMtx);
	for (auto& p : g_patched)
		if (p.vmt == vmt) return p.origPresent;
	return nullptr;
}

static fnResizeBuffers FindOrigResize(IDXGISwapChain* sc)
{
	void** vmt = *reinterpret_cast<void***>(sc);
	std::lock_guard<std::mutex> lk(g_vmtMtx);
	for (auto& p : g_patched)
		if (p.vmt == vmt) return p.origResize;
	return nullptr;
}

// ── Game WndProc subclass ──
static LRESULT CALLBACK Hk_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (g_ready && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	// Toggle key: INSERT shows/hides. Swallow it from the game.
	if (msg == WM_KEYDOWN && wParam == VK_INSERT)
	{
		guiVisible = !guiVisible;
		return 0;
	}

	return CallWindowProcW(g_origWndProc, hWnd, msg, wParam, lParam);
}

// ── The hooks ──
HRESULT STDMETHODCALLTYPE Hk_ResizeBuffers(IDXGISwapChain* sc, UINT bc, UINT w, UINT h, DXGI_FORMAT f, UINT flags)
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
	g_resetPending = true;
	fnResizeBuffers orig = FindOrigResize(sc);
	return orig ? orig(sc, bc, w, h, f, flags) : E_FAIL;
}

HRESULT STDMETHODCALLTYPE Hk_Present(IDXGISwapChain* sc, UINT sync, UINT flags)
{
	fnPresent orig = FindOrigPresent(sc);
	if (!orig) return E_FAIL;   // unknown vtable — shouldn't happen

	// Grab device/params on first call, initialize once.
	if (!g_ready)
	{
		sc->GetDevice(__uuidof(g_pd3dDevice), (void**)&g_pd3dDevice);
		if (!g_pd3dDevice)
		{
			static bool warned = false;
			if (!warned) { warned = true; OvLog("[ov] Present hook hit but GetDevice failed"); }
			return orig(sc, sync, flags);
		}
		g_pd3dDevice->GetImmediateContext(&g_pd3dDeviceContext);

		DXGI_SWAP_CHAIN_DESC d{};
		sc->GetDesc(&d);
		HWND hwnd = d.OutputWindow;
		if (!hwnd)
		{
			OvLog("[ov] Present hook hit but no OutputWindow");
			return orig(sc, sync, flags);
		}

		// Subclass window proc for input
		g_origWndProc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)&Hk_WndProc);

		g_hwnd = hwnd;
		g_injected = true;
		OvLog("[ov] first Present -> GuiInit");
		GuiInit(hwnd);
		CreateRTV(sc);
		g_ready = true;
		OvLog(g_mainRenderTargetView ? "[ov] RTV created, overlay live" : "[ov] RTV creation FAILED");
	}

	if (g_resetPending)
	{
		CreateRTV(sc);
		g_resetPending = false;
	}

	// Async INSERT poll — works even if the game eats/skips our WndProc subclass.
	{
		static bool s_prevIns = false;
		bool ins = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
		if (ins && !s_prevIns) guiVisible = !guiVisible;
		s_prevIns = ins;
	}

	red::PumpGameThread();

	if (guiVisible && g_mainRenderTargetView)
	{
		GuiRender();
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	return orig(sc, sync, flags);
}

void Uninstall()
{
	std::lock_guard<std::mutex> lk(g_vmtMtx);
	for (auto& p : g_patched)
	{
		DWORD old = 0;
		if (VirtualProtect(p.vmt, sizeof(void*) * 32, PAGE_READWRITE, &old))
		{
			p.vmt[8]  = reinterpret_cast<void*>(p.origPresent);
			p.vmt[13] = reinterpret_cast<void*>(p.origResize);
			VirtualProtect(p.vmt, sizeof(void*) * 32, old, &old);
		}
	}
	g_patched.clear();
	if (g_origWndProc && g_hwnd)
		SetWindowLongPtrW(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
}

} // namespace ov
