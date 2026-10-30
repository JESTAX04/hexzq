#pragma once
#include <windows.h>
#include <cstdint>

// ── Minimal x64 inline detour engine (14-byte absolute: FF 25 00 00 00 00 <addr8>) ──
struct Detour {
	uint8_t*       target = nullptr;      // original function
	uint8_t*       trampoline = nullptr;  // original-bytes + jmp back (call this for original behavior)
	uint8_t        original[64] = {};     // displaced bytes, len of them are live
	uint8_t*       patchSlot = nullptr;   // the 8-byte address slot inside our jmp
	size_t         len = 0;               // bytes displaced + patched (>= 14)
};

bool Detour_Attach(Detour& d, void* target, void* hook);
bool Detour_Detach(Detour& d);
