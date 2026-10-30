// detour.cpp — self-contained x64 detour (no MinHook).
// Strategy: overwrite target with `jmp [rip+0]` + 8-byte absolute addr.
// Trampoline: copy displaced bytes (instruction-length decode for the common prologues)
// then `jmp [rip+0]` back to target+len. Safe for normal compiler-emitted prologues.
#include "detour.h"
#include <cstring>

// Decode the length of one x86-64 instruction. Covers the prologue/push/sub patterns
// seen in Lua/dxgi function entries; returns 0 on anything exotic (caller bails).
static size_t InstrLen(const uint8_t* p)
{
	const uint8_t* s = p;
	uint8_t op;
	bool lockish = false;
	for (;;) {
		op = *p;
		if (op == 0x66 || op == 0x67 || op == 0xF0 || op >= 0x20 && op <= 0x3F && op != 0x26 && op != 0x2E && op != 0x36 && op != 0x3E) { ++p; }
		else if (op == 0xF2 || op == 0xF3) { ++p; }
		else break;
	}
	(void)lockish;
	switch (op)
	{
	case 0x50: case 0x51: case 0x52: case 0x53: case 0x55: case 0x56: case 0x57:
	case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5D: case 0x5E: case 0x5F:
	case 0x90: case 0xC3: case 0xC9:
		return (size_t)(p - s) + 1;
	case 0xE8: return (size_t)(p - s) + 5;            // call rel32
	case 0xE9: return (size_t)(p - s) + 5;            // jmp rel32
	case 0xEB: return (size_t)(p - s) + 2;            // jmp rel8
	case 0xCC: return (size_t)(p - s) + 1;            // int3
	case 0x48: case 0x49:                             // REX
	{
		++p;
		if (p >= s + 15) return 0;
		uint8_t o2 = *p;
		if (o2 >= 0x50 && o2 <= 0x5F) return (size_t)(p - s) + 1;         // push/pop r64
		if (o2 == 0x89 || o2 == 0x8B || o2 == 0x8D || o2 == 0x85 || o2 == 0x87) // mov/lea/xchg r/m
		{
			++p;
			uint8_t modrm = *p; ++p;
			uint8_t mod = modrm >> 6, rm = modrm & 7, reg = (modrm >> 3) & 7; (void)reg;
			if (mod == 3) return (size_t)(p - s);
			if (mod == 0 && rm == 5) return (size_t)(p - s) + 4;          // disp32 RIP-rel
			if (mod == 0 && rm == 4) { ++p; return (size_t)(p - s); }     // SIB
			if (mod == 1) { uint8_t sib = (rm == 4) ? *(p) : 0; (void)sib; ++p; if (rm == 4) ++p; return (size_t)(p - s); }
			if (mod == 2) { ++p; if (rm == 4) ++p; return (size_t)(p - s) + 4; }
		}
		if (o2 == 0xB8 || o2 == 0xB9 || o2 == 0xBA || o2 == 0xBB ||
			o2 == 0xBC || o2 == 0xBD || o2 == 0xBE || o2 == 0xBF)         // movabs r64, imm64
			return (size_t)(p - s) + 10;
		if (o2 == 0x05) return (size_t)(p - s) + 7;                        // add rax, imm32
		if (o2 == 0x2D) return (size_t)(p - s) + 7;                        // sub rax, imm32
		if (o2 == 0x25) return (size_t)(p - s) + 7;                        // and rax, imm32
		if (o2 == 0x3D) return (size_t)(p - s) + 7;                        // cmp rax, imm32
		if (o2 == 0xE9) return (size_t)(p - s) + 5;                        // jmp rel32 (REX)
		if (o2 == 0xFF)                                                   // call/jmp /2 /4
		{
			++p; uint8_t modrm = *p; ++p;
			uint8_t mod = modrm >> 6, rm = modrm & 7;
			if (mod == 3) return (size_t)(p - s);
			if (mod == 0 && rm == 5) return (size_t)(p - s) + 4;
			if (mod == 0 && rm == 4) { ++p; return (size_t)(p - s); }
			if (mod == 1) { ++p; if (rm == 4) ++p; return (size_t)(p - s); }
			if (mod == 2) { ++p; if (rm == 4) ++p; return (size_t)(p - s) + 4; }
		}
		if (o2 == 0x83 || o2 == 0x81)                                     // group: add/or/adc/sbb/and/sub/xor/cmp
		{
			++p; uint8_t modrm = *p; ++p;
			uint8_t mod = modrm >> 6, rm = modrm & 7;
			size_t imm = (o2 == 0x83) ? 1 : 4;
			if (mod == 3) return (size_t)(p - s) + imm;
			if (mod == 0 && rm == 5) return (size_t)(p - s) + 4 + imm;
			if (mod == 0 && rm == 4) { ++p; return (size_t)(p - s) + imm; }
			if (mod == 1) { ++p; if (rm == 4) ++p; return (size_t)(p - s) + imm; }
			if (mod == 2) { ++p; if (rm == 4) ++p; return (size_t)(p - s) + 4 + imm; }
		}
		if (o2 == 0x0F) goto two_byte;
		if (o2 == 0x39 || o2 == 0x3B)                                     // cmp r/m
		{
			++p; uint8_t modrm = *p; ++p;
			uint8_t mod = modrm >> 6, rm = modrm & 7;
			if (mod == 3) return (size_t)(p - s);
			if (mod == 0 && rm == 5) return (size_t)(p - s) + 4;
			if (mod == 0 && rm == 4) { ++p; return (size_t)(p - s); }
			if (mod == 1) { ++p; if (rm == 4) ++p; return (size_t)(p - s); }
			if (mod == 2) { ++p; if (rm == 4) ++p; return (size_t)(p - s) + 4; }
		}
		if (o2 == 0x8B || o2 == 0x89) return 0; // already handled above; unreachable
		return 0;
	}
	case 0x0F:
	{
	two_byte:;
		if (p >= s + 15) return 0;
		++p;
		uint8_t o2 = *p; ++p;
		if (o2 == 0x1F)                                                   // multi-byte nop
		{
			uint8_t modrm = *p; ++p;
			uint8_t mod = modrm >> 6, rm = modrm & 7;
			if (mod == 3) return (size_t)(p - s);
			if (mod == 0 && rm == 4) { ++p; return (size_t)(p - s); }
			if (mod == 0 && rm == 5) return (size_t)(p - s) + 4;
			return 0;
		}
		if (o2 == 0xB6 || o2 == 0xB7 || o2 == 0xBE || o2 == 0xBF)         // movzx/movsx r,r/m8
		{
			uint8_t modrm = *p; ++p;
			uint8_t mod = modrm >> 6, rm = modrm & 7;
			if (mod == 3) return (size_t)(p - s);
			if (mod == 0 && rm == 5) return (size_t)(p - s) + 4;
			if (mod == 0 && rm == 4) { ++p; return (size_t)(p - s); }
			if (mod == 1) { ++p; if (rm == 4) ++p; return (size_t)(p - s); }
			if (mod == 2) { ++p; if (rm == 4) ++p; return (size_t)(p - s) + 4; }
		}
		if (o2 >= 0x80 && o2 <= 0x8F)                                     // jcc rel32
			return (size_t)(p - s) + 4;
		return 0;
	}
	case 0x74: case 0x75:                                                 // je/jne rel8
		return (size_t)(p - s) + 2;
	case 0x83: case 0x81:                                                 // group w/o REX
	{
		++p; uint8_t modrm = *p; ++p;
		uint8_t mod = modrm >> 6, rm = modrm & 7;
		size_t imm = (op == 0x83) ? 1 : 4;
		if (mod == 3) return (size_t)(p - s) + imm;
		if (mod == 0 && rm == 5) return (size_t)(p - s) + 4 + imm;
		if (mod == 0 && rm == 4) { ++p; return (size_t)(p - s) + imm; }
		if (mod == 1) { ++p; if (rm == 4) ++p; return (size_t)(p - s) + imm; }
		if (mod == 2) { ++p; if (rm == 4) ++p; return (size_t)(p - s) + 4 + imm; }
		return 0;
	}
	case 0x89: case 0x8B: case 0x8D:                                      // mov/lea r/m (non-REX)
	{
		++p; uint8_t modrm = *p; ++p;
		uint8_t mod = modrm >> 6, rm = modrm & 7;
		if (mod == 3) return (size_t)(p - s);
		if (mod == 0 && rm == 5) return (size_t)(p - s) + 4;
		if (mod == 0 && rm == 4) { ++p; return (size_t)(p - s); }
		if (mod == 1) { ++p; if (rm == 4) ++p; return (size_t)(p - s); }
		if (mod == 2) { ++p; if (rm == 4) ++p; return (size_t)(p - s) + 4; }
		return 0;
	}
	case 0xB9: case 0xBA: case 0xBB:                                      // mov r32, imm32
		return (size_t)(p - s) + 5;
	case 0xA1: return (size_t)(p - s) + 9;
	default:
		return 0;
	}
}

bool Detour_Attach(Detour& d, void* target, void* hook)
{
	if (!target || !hook) return false;
	d.target = (uint8_t*)target;

	// Decode until we have >= 14 bytes of whole instructions
	size_t len = 0;
	while (len < 14) {
		size_t n = InstrLen(d.target + len);
		if (n == 0) return false;
		len += n;
		if (len > 64) return false;
	}
	d.len = len;   // record how much we displaced so Detach can restore all of it

	// Trampoline: RWX page within ±2GB for a rel32 jmp back (here: absolute for safety)
	uint8_t* tramp = (uint8_t*)VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!tramp) return false;

	DWORD old;
	if (!VirtualProtect(d.target, len, PAGE_EXECUTE_READWRITE, &old)) { VirtualFree(tramp, 0, MEM_RELEASE); return false; }

	memcpy(d.original, d.target, len);
	memcpy(tramp, d.target, len);

	// jmp tramp+len -> back into target+len  (FF 25 00 00 00 00 <addr>)
	uint8_t* back = tramp + len;
	back[0] = 0xFF; back[1] = 0x25;
	*(uint32_t*)(back + 2) = 0;
	*(uint64_t*)(back + 6) = (uint64_t)(d.target + len);

	// Patch target: jmp hook
	d.target[0] = 0xFF; d.target[1] = 0x25;
	*(uint32_t*)(d.target + 2) = 0;
	*(uint64_t*)(d.target + 6) = (uint64_t)hook;
	// Fill the rest with int3 in case displaced region is longer than 14
	for (size_t i = 14; i < len; ++i) d.target[i] = 0xCC;

	VirtualProtect(d.target, len, old, &old);
	FlushInstructionCache(GetCurrentProcess(), d.target, len);

	d.trampoline = tramp;
	return true;
}

bool Detour_Detach(Detour& d)
{
	if (!d.target || d.len == 0) return false;
	DWORD old;
	if (!VirtualProtect(d.target, d.len, PAGE_EXECUTE_READWRITE, &old)) return false;
	memcpy(d.target, d.original, d.len);
	VirtualProtect(d.target, d.len, old, &old);
	FlushInstructionCache(GetCurrentProcess(), d.target, d.len);
	if (d.trampoline) VirtualFree(d.trampoline, 0, MEM_RELEASE);
	d = {};
	return true;
}
