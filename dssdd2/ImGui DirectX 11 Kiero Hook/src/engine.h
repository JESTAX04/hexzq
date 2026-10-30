#pragma once
#include <string>
#include <cstdio>
#include <windows.h>

// ── Executor engine contract ──
// The UI (ui host: preview exe or injected DLL) talks to the Lua engine only
// through this header. engine_stub.cpp implements it for the preview host;
// src/lua_engine.cpp implements it for the injected build.

namespace red {

struct EngineStatus {
	bool attached = false;   // lua_State captured & executable
	const char* detail = "unknown";
};

// UI thread → engine
EngineStatus Status();
void EnqueueExecute(const std::string& code);   // thread-safe
bool PopConsoleLine(std::string& out);          // thread-safe, false when empty

// engine → UI console
void ConsoleLog(const std::string& line);       // thread-safe from any thread

// Game-thread pump: drains the execute queue on the correct thread.
// Injected build: called from the Present hook. Preview: no-op echo backend.
void PumpGameThread();
void InitEngine();                              // called once at startup/host attach

// Small file helpers shared by both builds
inline std::string WideToUtf8(const wchar_t* w) {
	int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
	if (n <= 0) return {};
	std::string s((size_t)n - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
	return s;
}

inline bool ReadFileText(const wchar_t* path, std::string& out) {
	FILE* f = _wfopen(path, L"rb");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0) { fclose(f); return false; }
	out.resize((size_t)sz);
	size_t got = fread(out.data(), 1, (size_t)sz, f);
	fclose(f);
	out.resize(got);
	return true;
}

inline bool WriteFileText(const wchar_t* path, const std::string& content) {
	FILE* f = _wfopen(path, L"wb");
	if (!f) return false;
	fwrite(content.data(), 1, content.size(), f);
	fclose(f);
	return true;
}

} // namespace red
