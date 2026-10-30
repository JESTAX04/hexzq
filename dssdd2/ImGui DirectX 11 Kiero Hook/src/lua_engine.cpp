// lua_engine.cpp — injected backend for the red:: engine contract (real Lua 5.4 VM hijack).
//
// Architecture:
//   1. Find the module hosting the embedded Lua 5.4 VM (export scan + banner-string scan).
//   2. Resolve a minimal Lua C-API (exports first, sig fallback).
//   3. Detour lua_pcallk. The FIRST call captures the live lua_State — and crucially,
//      every queued chunk executes INSIDE the hook (correct scripting thread, no
//      cross-thread VM races).
//   4. `print` in the target state is redirected into our console ring.
//
// Everything that touches the foreign VM is SEH-wrapped: a bad pointer logs and
// survives instead of taking the game down.
#include "engine.h"
#include "sigscan.h"
#include "detour.h"
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

#include <deque>
#include <mutex>
#include <string>

// ── Minimal Lua 5.4 ABI (x64 cdecl) — no lua.h needed ──
struct lua_State;
typedef long long lua_KContext;
typedef int  (*lua_KFunction)(lua_State*, int, lua_KContext);
typedef int  (*lua_CFunction)(lua_State*);

#define LUA_MULTRET (-1)

typedef int  (*fn_luaL_loadbufferx)(lua_State*, const char*, size_t, const char*, const char*);
typedef int  (*fn_lua_pcallk)(lua_State*, int, int, int, lua_KContext, lua_KFunction);
typedef const char* (*fn_lua_tostring)(lua_State*, int);
typedef void (*fn_lua_settop)(lua_State*, int);
typedef int  (*fn_lua_gettop)(lua_State*);
typedef int  (*fn_lua_pushcfunction)(lua_State*, lua_CFunction);
typedef void (*fn_lua_setglobal)(lua_State*, const char*);
typedef void (*fn_lua_pushstring)(lua_State*, const char*);

namespace red {

// ───────────────────────────── console + queue ─────────────────────────────
static std::mutex              g_mtx;
static std::deque<std::string> g_queue;
static std::deque<std::string> g_console;
static bool                    g_consoleHooked = false;

static void PushConsoleLocked(const std::string& line)
{
	g_console.push_back(line);
	if (g_console.size() > 800)
		g_console.erase(g_console.begin(), g_console.begin() + 200);
}

void ConsoleLog(const std::string& line)
{
	std::lock_guard<std::mutex> lk(g_mtx);
	PushConsoleLocked(line);
}

bool PopConsoleLine(std::string& out)
{
	std::lock_guard<std::mutex> lk(g_mtx);
	if (g_console.empty()) return false;
	out = std::move(g_console.front());
	g_console.pop_front();
	return true;
}

void EnqueueExecute(const std::string& code)
{
	std::lock_guard<std::mutex> lk(g_mtx);
	g_queue.push_back(code);
}

// ───────────────────────────── resolved API ─────────────────────────────
struct LuaApi {
	uintptr_t modBase = 0;
	size_t    modSize = 0;
	wchar_t   modName[64] = L"";

	void* pcallk = nullptr;
	fn_luaL_loadbufferx  loadbuffer = nullptr;
	fn_lua_pcallk        pcall = nullptr;
	fn_lua_tostring      tostring = nullptr;
	fn_lua_settop        settop = nullptr;
	fn_lua_gettop        gettop = nullptr;
	fn_lua_pushcfunction pushcfn = nullptr;
	fn_lua_pushstring    pushstr = nullptr;
	fn_lua_setglobal     setglobal = nullptr;

	bool Ready() const { return pcallk && loadbuffer && pcall && tostring && settop && pushcfn && pushstr && setglobal; }
};

static LuaApi   g_api;
static lua_State* g_L = nullptr;          // live VM state, captured from pcallk hook
static bool     g_attached = false;
static const char* g_status = "boot";
static char     g_statusDetail[192] = "boot";

// debug log for sig-retargeting (never verbose on success path)
static void Dbg(const std::string& s)
{
	char path[MAX_PATH];
	GetTempPathA(MAX_PATH, path);
	strcat_s(path, "redengine_debug.log");
	FILE* f = nullptr;
	fopen_s(&f, path, "a");
	if (f) { fputs(s.c_str(), f); fputc('\n', f); fclose(f); }
}

// ───────────────────────── SEH-wrapped VM ops ─────────────────────────
// MSVC C2712: frames containing __try must have NO destructible objects.
// All reporting helpers below are POD-only; string work happens in outer frames.
static bool SafeToString(lua_State* L, int idx, const char*& out)
{
	__try
	{
		out = g_api.tostring(L, idx);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { out = nullptr; return false; }
}

static int SafeGettop(lua_State* L)
{
	__try { return g_api.gettop(L); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static void SafeSettop0(lua_State* L)
{
	__try { g_api.settop(L, 0); }
	__except (EXCEPTION_EXECUTE_HANDLER) {}
}

static bool SafeInstallPrint(lua_State* L, lua_CFunction fn)
{
	__try
	{
		g_api.pushcfn(L, fn);
		g_api.setglobal(L, "print");
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ── POD reporting helpers (constructed in non-__try frames) ──
static void LogPre(const char* prefix, const char* msg)
{
	ConsoleLog(std::string(prefix) + (msg && *msg ? msg : "(none)"));
}
static void LogRaw(const char* msg)
{
	ConsoleLog(std::string(msg));
}

// print(...) → our console. Raw capture frame; string building in RedPrint's outer frame.
static void RedPrintCore(lua_State* L, char* line, size_t cap)
{
	line[0] = 0;
	size_t used = 0;
	const int n = SafeGettop(L);

	__try
	{
		for (int i = 1; i <= n && used < cap - 2; ++i)
		{
			const char* s = g_api.tostring(L, i);
			if (!s) { line[used++] = '?'; continue; }
			size_t l = strlen(s);
			if (l > cap - used - 2) l = cap - used - 2;
			memcpy(line + used, s, l);
			used += l;
			if (i < n) line[used++] = '\t';
		}
		line[used] = 0;
		g_api.settop(L, 0);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { line[0] = 0; }
}

static int RedPrint(lua_State* L)
{
	static char line[4096];
	RedPrintCore(L, line, sizeof(line));
	if (line[0]) LogPre("[lua] ", line);
	return 0;
}

// ───────────────────────── pcallk detour ─────────────────────────
static Detour g_pcallDetour = {};
static volatile LONG g_executing = 0;

typedef int (*fn_lua_pcallk_orig)(lua_State*, int, int, int, lua_KContext, lua_KFunction);

struct ChunkResult {
	int  stage;          // 0=exec-ok 1=compile-fail 2=runtime-error 3=hard-fault
	char msg[512];
};

// Runs one queued chunk on the live VM. POD-only frame.
static void RunChunkCore(lua_State* L, const char* buf, size_t len, ChunkResult& res)
{
	res.stage = 0;
	res.msg[0] = 0;
	int err = 0;
	int rc = 0;

	__try
	{
		err = g_api.loadbuffer(L, buf, len, "=(redengine)", "t");
		if (err != 0)
		{
			res.stage = 1;
			const char* m = g_api.tostring(L, -1);
			if (m) { strncpy_s(res.msg, m, _TRUNCATE); }
		}
		else
		{
			rc = g_api.pcall(L, 0, LUA_MULTRET, 0, 0, nullptr);
			if (rc != 0)
			{
				res.stage = 2;
				const char* m = g_api.tostring(L, -1);
				if (m) { strncpy_s(res.msg, m, _TRUNCATE); }
			}
		}
		g_api.settop(L, 0);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		res.stage = 3;
	}
}

static void RunChunkReport(lua_State* L, const char* buf, size_t len)
{
	ChunkResult res;
	RunChunkCore(L, buf, len, res);
	switch (res.stage)
	{
	case 0: LogRaw("[ok] executed"); break;
	case 1: LogPre("[compile] ", res.msg); break;
	case 2: LogPre("[error] ", res.msg); break;
	case 3: LogRaw("[error] protected-mode fault during execute"); SafeSettop0(L); break;
	}
}

static int HK_lua_pcallk(lua_State* L, int nargs, int nresults, int msgh, lua_KContext ctx, lua_KFunction k)
{
	// First hit: capture the live state + redirect print.
	if (!g_L)
	{
		g_L = L;
		if (SafeInstallPrint(L, &RedPrint))
			LogRaw("[red] VM state captured, console live");
	}

	// Execute pending chunks here: correct thread, live VM, one at a time.
	if (InterlockedCompareExchange(&g_executing, 1, 0) == 0)
	{
		std::string chunk;
		bool have = false;
		{
			std::lock_guard<std::mutex> lk(g_mtx);
			if (!g_queue.empty()) { chunk = std::move(g_queue.front()); g_queue.pop_front(); have = true; }
		}
		if (have)
			RunChunkReport(L, chunk.c_str(), chunk.size());
		InterlockedExchange(&g_executing, 0);
	}

	fn_lua_pcallk_orig orig = (fn_lua_pcallk_orig)g_pcallDetour.trampoline;
	return orig(L, nargs, nresults, msgh, ctx, k);
}

// ───────────────────────── resolution ─────────────────────────
static bool TryExport(HMODULE h, const char* name, void*& out)
{
	void* p = (void*)GetProcAddress(h, name);
	if (!p) return false;
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(p, &mbi, sizeof(mbi)))
	{
		DWORD ok = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
		if (mbi.Protect & ok) { out = p; return true; }
	}
	return false;
}

// Find the module hosting the Lua VM: exports first, banner strings as proof.
static HMODULE FindLuaModule()
{
	// Pass 1: any module exporting the C API
	DWORD cnt = 0;
	HMODULE mods[1024];
	DWORD needed = 0;
	if (!K32EnumProcessModulesEx(GetCurrentProcess(), mods, sizeof(mods), &needed, LIST_MODULES_ALL))
		return nullptr;
	cnt = needed / sizeof(HMODULE);

	for (DWORD i = 0; i < cnt; ++i)
	{
		void* p = nullptr;
		if (TryExport(mods[i], "lua_pcallk", p))
			return mods[i];
	}

	// Pass 2: banner/string scan for stripped runtimes ("Lua 5.4" + api error literals)
	for (DWORD i = 0; i < cnt; ++i)
	{
		uintptr_t base = 0; size_t size = 0;
		WCHAR nm[64] = L"";
		GetModuleFileNameExW(GetCurrentProcess(), mods[i], nm, 64);
		base = (uintptr_t)mods[i];

		IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
		if (dos->e_magic != IMAGE_DOS_SIGNATURE) continue;
		IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE) continue;
		size = nt->OptionalHeader.SizeOfImage;

		auto secs = sig::ExecSections(base);
		bool hit54 = false, hitApi = false;
		for (auto& s : secs)
		{
			if (!hit54 && sig::FindString(s.addr, s.size, "Lua 5.4")) hit54 = true;
			if (!hitApi && sig::FindString(s.addr, s.size, "attempt to yield a corrupted value")) hitApi = true;
			if (hit54 && hitApi) break;
		}
		// also .rdata scan (strings usually live there)
		{
			IMAGE_SECTION_HEADER* sh = IMAGE_FIRST_SECTION(nt);
			for (WORD k = 0; k < nt->FileHeader.NumberOfSections; ++k, ++sh)
			{
				uint8_t* a = (uint8_t*)base + sh->VirtualAddress;
				size_t   z = sh->Misc.VirtualSize;
				if (a < (uint8_t*)base || z == 0 || z > size) continue;
				if (!hit54 && sig::FindString(a, z, "Lua 5.4")) hit54 = true;
				if (!hitApi && sig::FindString(a, z, "stack underflow")) hitApi = true;
			}
		}
		if (hit54 && hitApi)
			return mods[i];
	}
	return nullptr;
}

// Signature table for MSVC-built Lua 5.4 (retarget per binary; debug dump assists).
// Each entry: fallback patterns, most specific first.
static const char* SIG_LOADBUFFER[] = {
	"48 89 5C 24 ? 57 48 83 EC ? 49 8B F9 48 8B DA 48 8B F1 8B 49 ?",
	"40 53 56 57 48 83 EC ? 48 8B F1 49 8B F0 48 8B DA 45 33 C9",
	nullptr
};
static const char* SIG_TOSTRING[] = {
	"48 89 5C 24 ? 57 48 83 EC ? 48 8B D9 48 85 C9 74 ? 8B 49 ?",
	"40 53 56 57 48 83 EC ? 48 8B F1 0F 57 C0 48 85 C9 74 ?",
	nullptr
};
static const char* SIG_SETTOP[] = {
	"48 89 5C 24 ? 57 48 8B F9 E8 ? ? ? ? 8B 4F ? 3B CA 7D ?",
	"48 8B C1 48 85 C0 74 ? 8B 51 ?",
	nullptr
};
static const char* SIG_GETTOP[] = {
	"8B 41 ? C3",
	"8B 41 ? 0F AF C0 0F AF C0 C3",
	nullptr
};
static const char* SIG_PUSHCFN[] = {
	"48 89 5C 24 ? 57 48 83 EC ? 48 8B F9 48 8B DA 48 85 C9 74 ?",
	"40 53 56 57 48 83 EC ? 48 8B F1 48 8B FA 48 85 C9 0F 84 ?",
	nullptr
};
static const char* SIG_PUSHSTR[] = {
	"48 89 5C 24 ? 57 48 83 EC ? 48 8B F9 48 8B DA 48 85 C9 0F 84 ?",
	"40 53 56 57 48 83 EC ? 48 8B F1 48 8B FA 48 85 C9 0F 84 ?",
	nullptr
};
static const char* SIG_SETGLOBAL[] = {
	"48 89 5C 24 ? 48 89 6C 24 ? 56 57 48 83 EC ? 48 8B F9 48 8B F2 48 8B DA E8 ?",
	"40 53 56 57 41 56 48 83 EC ? 48 8B F1 48 8B F2 48 8B FA E8 ?",
	nullptr
};
static const char* SIG_PCALLK[] = {
	"48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B 41 ? 8B 51 ? 8B F9 41 0F B6 E8",
	"48 89 5C 24 ? 57 48 83 EC ? 8B 49 ? 8B 51 ? 8B F9 41 0F B6 E8",
	nullptr
};

static void* SigOrExport(HMODULE h, const char* exportName, const char** sigs)
{
	void* p = nullptr;
	if (exportName && TryExport(h, exportName, p)) return p;
	if (h && sigs)
	{
		auto secs = sig::ExecSections((uintptr_t)h);
		for (auto& s : secs)
			for (int i = 0; sigs[i]; ++i)
				if (void* m = sig::Scan(s.addr, s.size, sigs[i]))
					return m;
	}
	return nullptr;
}

static void DumpCandidates(HMODULE h)
{
	Dbg("=== sig retarget dump ===");
	wchar_t nm[64] = L"";
	GetModuleFileNameExW(GetCurrentProcess(), h, nm, 64);
	Dbg("module: " + [&]{ std::string s; for (wchar_t* p = nm; *p; ++p) s += (char)*p; return s; }());
	struct { const char* name; const char** sigs; } tbl[] = {
		{ "luaL_loadbufferx", SIG_LOADBUFFER }, { "lua_tostring", SIG_TOSTRING },
		{ "lua_settop", SIG_SETTOP }, { "lua_gettop", SIG_GETTOP },
		{ "lua_pushcfunction", SIG_PUSHCFN }, { "lua_pushstring", SIG_PUSHSTR },
		{ "lua_setglobal", SIG_SETGLOBAL }, { "lua_pcallk", SIG_PCALLK },
	};
	auto secs = sig::ExecSections((uintptr_t)h);
	for (auto& t : tbl)
	{
		bool found = false;
		for (auto& s : secs)
			for (int i = 0; t.sigs[i]; ++i)
				if (void* m = sig::Scan(s.addr, s.size, t.sigs[i]))
				{
					Dbg(std::string(t.name) + " MATCH pattern#" + std::to_string(i));
					found = true; break;
				}
		if (!found) Dbg(std::string(t.name) + " NO MATCH — retarget needed");
	}
	Dbg("=========================");
}

static bool ResolveApi()
{
	HMODULE h = FindLuaModule();
	if (!h) { g_status = "no-lua"; strncpy_s(g_statusDetail, "Lua 5.4 module not found in process", _TRUNCATE); Dbg("FindLuaModule failed"); return false; }

	g_api.modBase = (uintptr_t)h;
	GetModuleFileNameExW(GetCurrentProcess(), h, g_api.modName, 64);

	void* p;
	g_api.pcallk    = SigOrExport(h, "lua_pcallk", SIG_PCALLK);
	g_api.loadbuffer = (fn_luaL_loadbufferx)SigOrExport(h, "luaL_loadbufferx", SIG_LOADBUFFER);
	g_api.pcall      = (fn_lua_pcallk)(g_api.pcallk ? g_api.pcallk : SigOrExport(h, "lua_pcall", nullptr));
	g_api.tostring   = (fn_lua_tostring)SigOrExport(h, "lua_tostring", SIG_TOSTRING);
	g_api.settop     = (fn_lua_settop)SigOrExport(h, "lua_settop", SIG_SETTOP);
	g_api.gettop     = (fn_lua_gettop)SigOrExport(h, "lua_gettop", SIG_GETTOP);
	g_api.pushcfn    = (fn_lua_pushcfunction)SigOrExport(h, "lua_pushcfunction", SIG_PUSHCFN);
	g_api.pushstr    = (fn_lua_pushstring)SigOrExport(h, "lua_pushstring", SIG_PUSHSTR);
	g_api.setglobal  = (fn_lua_setglobal)SigOrExport(h, "lua_setglobal", SIG_SETGLOBAL);
	(void)p;

	if (!g_api.gettop) g_api.gettop = [](lua_State*) -> int { return 1; }; // non-fatal

	if (!g_api.Ready())
	{
		g_status = "retarget";
		strncpy_s(g_statusDetail, "VM located - signature retarget needed (see %TEMP%\\redengine_debug.log)", _TRUNCATE);
		Dbg("API incomplete; dumping candidate states");
		DumpCandidates(h);
		return false;
	}
	return true;
}

static bool Attach()
{
	if (!ResolveApi()) return false;
	if (!Detour_Attach(g_pcallDetour, g_api.pcallk, (void*)&HK_lua_pcallk))
	{
		g_status = "detour-fail";
		strncpy_s(g_statusDetail, "detour attach failed on lua_pcallk", _TRUNCATE);
		Dbg("Detour_Attach failed");
		return false;
	}
	g_attached = true;
	g_status = "armed";
	strncpy_s(g_statusDetail, "hook armed - waiting for first VM call", _TRUNCATE);
	ConsoleLog("[red] lua hook armed, awaiting live state");
	return true;
}

// ───────────────────────── contract ─────────────────────────
EngineStatus Status()
{
	EngineStatus s;
	s.attached = g_attached && g_L != nullptr;
	if (!g_attached) { s.attached = false; s.detail = g_statusDetail; return s; }
	if (!g_L) { s.attached = false; s.detail = "hook armed, VM idle (trigger a script event)"; return s; }
	s.detail = "VM attached - execution live";
	return s;
}

void InitEngine()
{
	// If called before the game is settled (dllmain bootstrap), delay the attach.
	struct Boot { static DWORD WINAPI Wait(void* a) { Sleep(2500); Attach(); return 0; } };
	CreateThread(nullptr, 0, &Boot::Wait, nullptr, 0, nullptr);
}

void PumpGameThread()
{
	// Nothing to do on the render thread: chunks execute inside the pcallk hook.
}

} // namespace red
