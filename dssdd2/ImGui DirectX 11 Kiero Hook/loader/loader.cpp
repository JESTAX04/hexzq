// loader.cpp — redENGINE injector.
// FindTarget: window class → process; LoadLibrary remote-thread stage 1.
// build: cl /EHsc /O2 /std:c++20 loader.cpp /link user32.lib advapi32.lib /SUBSYSTEM:WINDOWS
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <cwchar>
#include <string>
#include <cstdio>

#pragma comment(lib, "psapi.lib")

static bool TargetAlreadyInjected(DWORD pid)
{
	HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if (!h) return false;
	HMODULE mods[512];
	DWORD needed = 0;
	bool found = false;
	if (K32EnumProcessModulesEx(h, mods, sizeof(mods), &needed, LIST_MODULES_ALL))
	{
		for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < 512; ++i)
		{
			wchar_t name[64] = L"";
			if (K32GetModuleBaseNameW(h, mods[i], name, 64) && wcsstr(name, L"redengine"))
			{ found = true; break; }
		}
	}
	CloseHandle(h);
	return found;
}

static HWND FindGameWindow()
{
	// Fast path: known window classes
	static const wchar_t* kClasses[] = {
		L"FXGRADESGAMEWINDOWEXCLASS",   // FiveM / GTA V (Cfx.re)
		L"GDPRewrite",                  // GTA V native
		L"VALORANT_UnrealWindow",       // (placeholder for future targets)
	};
	for (auto cls : kClasses)
		if (HWND h = FindWindowExW(nullptr, nullptr, cls, nullptr))
			return h;

	// Title scan
	struct Ctx { HWND best; } ctx{ nullptr };
	EnumWindows([](HWND h, LPARAM lp) -> BOOL {
		Ctx* c = (Ctx*)lp;
		if (!IsWindowVisible(h)) return TRUE;
		wchar_t title[256] = L"";
		GetWindowTextW(h, title, 256);
		if (wcsstr(title, L"FiveM") || wcsstr(title, L"Grand Theft Auto"))
		{ c->best = h; return FALSE; }
		return TRUE;
	}, (LPARAM)&ctx);
	return ctx.best;
}

static bool InjectDll(DWORD pid, const wchar_t* dllPath, std::wstring& err)
{
	HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
		PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
	if (!hProc) { err = L"OpenProcess failed (run elevated?)"; return false; }

	bool ok = false;
	SIZE_T sz = (wcslen(dllPath) + 1) * sizeof(wchar_t);
	void* mem = VirtualAllocEx(hProc, nullptr, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!mem) { err = L"VirtualAllocEx failed"; CloseHandle(hProc); return false; }

	SIZE_T written = 0;
	if (!WriteProcessMemory(hProc, mem, dllPath, sz, &written) || written != sz)
	{
		err = L"WriteProcessMemory failed";
		VirtualFreeEx(hProc, mem, 0, MEM_RELEASE);
		CloseHandle(hProc);
		return false;
	}

	HMODULE krn = GetModuleHandleW(L"kernel32.dll");
	auto loadLib = (LPTHREAD_START_ROUTINE)GetProcAddress(krn, "LoadLibraryW");
	if (!loadLib) { err = L"GetProcAddress LoadLibraryW failed"; VirtualFreeEx(hProc, mem, 0, MEM_RELEASE); CloseHandle(hProc); return false; }

	HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0, loadLib, mem, 0, nullptr);
	if (!hThread) { err = L"CreateRemoteThread failed"; VirtualFreeEx(hProc, mem, 0, MEM_RELEASE); CloseHandle(hProc); return false; }

	DWORD wait = WaitForSingleObject(hThread, 30000);
	if (wait == WAIT_TIMEOUT) err = L"LoadLibrary timeout (AV intercepting?)";
	else
	{
		DWORD exitCode = 0;
		GetExitCodeThread(hThread, &exitCode);
		if (exitCode == 0) err = L"LoadLibrary returned NULL (AV block / bad path)";
		else ok = true;
	}

	CloseHandle(hThread);
	VirtualFreeEx(hProc, mem, 0, MEM_RELEASE);
	CloseHandle(hProc);
	return ok;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	wchar_t path[MAX_PATH];
	GetModuleFileNameW(nullptr, path, MAX_PATH);
	wchar_t* slash = wcsrchr(path, L'\\');
	wcscpy_s(slash + 1, MAX_PATH - (slash - path) - 1, L"redengine.dll");
	if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
	{
		MessageBoxW(nullptr, L"redengine.dll not found next to loader.", L"redENGINE", MB_ICONERROR);
		return 1;
	}

	HWND hwnd = FindGameWindow();
	if (!hwnd)
	{
		MessageBoxW(nullptr, L"Game window not found. Launch FiveM first.", L"redENGINE", MB_ICONERROR);
		return 1;
	}

	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);

	if (TargetAlreadyInjected(pid))
	{
		MessageBoxW(nullptr,
			L"An OLD redengine.dll is still loaded in this game process.\n\n"
			L"Fully EXIT FiveM (close the game window, not just the session),\n"
			L"relaunch it, then run this loader again.",
			L"redENGINE", MB_ICONWARNING);
		return 2;
	}

	std::wstring err;
	if (!InjectDll(pid, path, err))
	{
		std::wstring msg = L"Injection failed: " + err;
		MessageBoxW(nullptr, msg.c_str(), L"redENGINE", MB_ICONERROR);
		return 1;
	}

	// Confirm the module actually landed.
	Sleep(300);
	const bool verified = TargetAlreadyInjected(pid);

	wchar_t okmsg[320];
	wsprintfW(okmsg,
		L"Injected into pid %lu%s. Wait ~5s after the loading screen, then press INSERT.",
		pid, verified ? L" (verified)" : L" [NOT VERIFIED - LoadLibrary returned but module missing]");
	MessageBoxW(nullptr, okmsg, L"redENGINE", verified ? MB_ICONINFORMATION : MB_ICONWARNING);
	return 0;
}
