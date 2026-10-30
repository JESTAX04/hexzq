// dllmain.cpp — injected entry point.
// Two independent ways to reach a live swapchain (belt + suspenders):
//   A) build a throwaway device+chain and VMT-patch Present(8)/ResizeBuffers(13)
//      (indices verified against Windows SDK dxgi.h via offsetof)
//   B) inline-detour dxgi!CreateSwapChain* exports so any chain created AFTER
//      injection also gets VMT-patched
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include "engine.h"
#include "detour.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace ov {
bool HookSwapchain(IDXGISwapChain* sc);
bool IsArmed();
void Uninstall();
}

static HWND g_tmpWnd = nullptr;
static HMODULE g_self = nullptr;

static void BootLog(const char* msg)
{
	char path[MAX_PATH];
	GetTempPathA(MAX_PATH, path);
	strcat_s(path, "redengine_debug.log");
	FILE* f = nullptr;
	fopen_s(&f, path, "a");
	if (f) { fputs(msg, f); fputc('\n', f); fclose(f); }
}

static LRESULT CALLBACK TmpProc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

// ── B) dxgi export detours ────────────────────────────────────────────
typedef HRESULT(STDMETHODCALLTYPE* fnCreateSwapChainForHwnd)(IDXGIFactory*, IUnknown*, HWND,
	const DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
typedef HRESULT(STDMETHODCALLTYPE* fnCreateSwapChain)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);

static fnCreateSwapChainForHwnd g_origCSHW = nullptr;
static fnCreateSwapChain        g_origCS   = nullptr;
static Detour g_dsh[2] = {};

static HRESULT STDMETHODCALLTYPE HK_CreateSwapChainForHwnd(IDXGIFactory* f, IUnknown* dev, HWND hwnd,
	const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fs, IDXGIOutput* o, IDXGISwapChain1** out)
{
	HRESULT hr = g_origCSHW(f, dev, hwnd, desc, fs, o, out);
	if (SUCCEEDED(hr) && out && *out)
	{
		BootLog("[boot] CreateSwapChainForHwnd intercepted -> VMT patch");
		ov::HookSwapchain(reinterpret_cast<IDXGISwapChain*>(*out));
	}
	return hr;
}

static HRESULT STDMETHODCALLTYPE HK_CreateSwapChain(IDXGIFactory* f, IUnknown* dev, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** out)
{
	HRESULT hr = g_origCS(f, dev, desc, out);
	if (SUCCEEDED(hr) && out && *out)
	{
		BootLog("[boot] CreateSwapChain intercepted -> VMT patch");
		ov::HookSwapchain(*out);
	}
	return hr;
}

static void HookDxgiExports()
{
	HMODULE dxgi = GetModuleHandleW(L"dxgi.dll");
	if (!dxgi)
	{
		BootLog("[boot] dxgi.dll not loaded yet");
		return;
	}

	void* p1 = (void*)GetProcAddress(dxgi, "CreateSwapChainForHwnd");
	void* p2 = (void*)GetProcAddress(dxgi, "CreateSwapChain");
	char m[160];
	wsprintfA(m, "[boot] dxgi exports CSHW=%p CS=%p", p1, p2);
	BootLog(m);

	if (p1)
	{
		g_origCSHW = (fnCreateSwapChainForHwnd)p1; // Detour fills trampoline; save first
		if (Detour_Attach(g_dsh[0], p1, (void*)&HK_CreateSwapChainForHwnd))
			g_origCSHW = (fnCreateSwapChainForHwnd)g_dsh[0].trampoline;
		else BootLog("[boot] detour CreateSwapChainForHwnd FAILED");
	}
	if (p2)
	{
		g_origCS = (fnCreateSwapChain)p2;
		if (Detour_Attach(g_dsh[1], p2, (void*)&HK_CreateSwapChain))
			g_origCS = (fnCreateSwapChain)g_dsh[1].trampoline;
		else BootLog("[boot] detour CreateSwapChain FAILED");
	}
}

// ── A) temp-chain bootstrap → VMT patch ──────────────────────────────
static HWND MakeTmpWindow()
{
	HINSTANCE hi = (HINSTANCE)g_self;
	WNDCLASSEXW wc = {};
	wc.cbSize        = sizeof(wc);
	wc.lpfnWndProc   = &TmpProc;
	wc.hInstance     = hi;              // SAME hInstance in register+create — mismatch
	wc.lpszClassName = L"RED_ENG_TMPCLS";// previously killed CreateWindowExW silently
	RegisterClassExW(&wc);

	HWND h = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, L"", WS_POPUP,
		0, 0, 16, 16, nullptr, nullptr, hi, nullptr);
	if (!h)
	{
		char msg[128];
		wsprintfA(msg, "[boot] tmp window FAILED gle=%lu", GetLastError());
		BootLog(msg);
	}
	return h;
}

static void TryTempChain(HWND outputWindow, const char* tag)
{
	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = outputWindow;
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	IDXGISwapChain* tmp = nullptr;
	ID3D11Device* dev = nullptr;
	ID3D11DeviceContext* ctx = nullptr;
	D3D_FEATURE_LEVEL fl;
	const D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
	HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
		fls, 2, D3D11_SDK_VERSION, &sd, &tmp, &dev, &fl, &ctx);
	if (FAILED(hr))
		hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
			fls, 2, D3D11_SDK_VERSION, &sd, &tmp, &dev, &fl, &ctx);

	char msg[192];
	wsprintfA(msg, "[boot] temp chain(%s) hr=0x%08X chain=%p", tag, (unsigned)hr, (void*)tmp);
	BootLog(msg);

	if (SUCCEEDED(hr) && tmp)
	{
		bool ok = ov::HookSwapchain(tmp);
		wsprintfA(msg, "[boot] VMT patch via temp(%s) -> %s", tag, ok ? "ARMED" : "FAIL");
		BootLog(msg);
	}

	if (tmp) tmp->Release();
	if (dev) dev->Release();
	if (ctx) ctx->Release();
}

// Flip-model chain (SwapChain1/4 class) — different internal vtable than DISCARD.
// The game's already-created chain lives on ONE of these classes; patch both.
static void TryFlipChain(HWND outputWindow)
{
	IDXGIFactory2* fact = nullptr;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory2), (void**)&fact)) || !fact)
	{
		BootLog("[boot] CreateDXGIFactory1 failed");
		return;
	}

	IDXGISwapChain1* chain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 d1 = {};
	d1.BufferCount = 2;
	d1.Width = 16; d1.Height = 16;
	d1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	d1.SampleDesc.Count = 1;
	d1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	ID3D11Device* dev = nullptr;
	ID3D11DeviceContext* ctx = nullptr;
	D3D_FEATURE_LEVEL fl;
	const D3D_FEATURE_LEVEL fls[] = { D3D_FEATURE_LEVEL_11_0 };
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fls, 1,
		D3D11_SDK_VERSION, &dev, &fl, &ctx);
	if (SUCCEEDED(hr))
		hr = fact->CreateSwapChainForHwnd(dev, outputWindow, &d1, nullptr, nullptr, &chain);

	char msg[128];
	wsprintfA(msg, "[boot] flip chain hr=0x%08X chain=%p", (unsigned)hr, (void*)chain);
	BootLog(msg);

	if (SUCCEEDED(hr) && chain)
	{
		bool ok = ov::HookSwapchain(reinterpret_cast<IDXGISwapChain*>(chain));
		wsprintfA(msg, "[boot] VMT patch via flip-class -> %s", ok ? "ARMED(+)" : "FAIL");
		BootLog(msg);
		chain->Release();
	}
	if (dev) dev->Release();
	if (ctx) ctx->Release();
	fact->Release();
}

static void BootstrapSeh()
{
	HookDxgiExports();            // (B) works for chains created from now on

	g_tmpWnd = MakeTmpWindow();   // (A) covers chains that ALREADY exist
	HWND out = g_tmpWnd;
	if (!out)
	{
		extern HWND g_hwnd;
		out = FindWindowExW(nullptr, nullptr, L"FXGRADESGAMEWINDOWEXCLASS", nullptr);
		if (!out) out = g_hwnd;
		if (!out) out = GetDesktopWindow();
	}
	if (out)
	{
		TryTempChain(out, "discard-class");
		TryFlipChain(out);        // patch both DXGI swapchain classes the game could be on
		char msg[96];
		wsprintfA(msg, "[boot] bootstrap done, armed=%d", ov::IsArmed() ? 1 : 0);
		BootLog(msg);
	}
	else BootLog("[boot] no output HWND available for bootstrap");
}

static void __stdcall BootstrapGuarded()
{
	__try
	{
		BootstrapSeh();
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		BootLog("[boot] Bootstrap EXCEPTION - overlay host broken");
	}
}

static DWORD WINAPI BootThread(LPVOID)
{
	BootLog("[boot] dll attached, boot thread started");
	// Let the process settle; runtime modules (the Lua VM host) load late.
	Sleep(2500);
	BootstrapGuarded();
	// Engine boot (lua sig-scan + pcallk detour) spawns its own deferred thread.
	red::InitEngine();
	return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		g_self = h;
		DisableThreadLibraryCalls(h);
		CreateThread(nullptr, 0, &BootThread, nullptr, 0, nullptr);
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		ov::Uninstall();
		if (g_tmpWnd) DestroyWindow(g_tmpWnd);
	}
	return TRUE;
}
