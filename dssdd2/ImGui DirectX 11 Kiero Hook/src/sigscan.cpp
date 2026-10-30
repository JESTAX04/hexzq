#include "sigscan.h"
#include <cstring>
#include <vector>

namespace sig {

static inline bool HexNibble(char c, int& out)
{
	if (c >= '0' && c <= '9') { out = c - '0'; return true; }
	if (c >= 'a' && c <= 'f') { out = c - 'a' + 10; return true; }
	if (c >= 'A' && c <= 'F') { out = c - 'A' + 10; return true; }
	return false;
}

static inline bool ParsePattern(const char* pattern, uint8_t bytes[256], bool mask[256], int& count)
{
	count = 0;
	const char* p = pattern;
	while (*p && count < 256)
	{
		while (*p == ' ') ++p;
		if (!*p) break;
		if (p[0] == '?')
		{
			mask[count] = false;
			bytes[count] = 0;
			++count;
			p += 1;
			if (p[0] == '?') ++p; // "?? " also allowed
			continue;
		}
		int hi, lo;
		if (!HexNibble(p[0], hi) || !HexNibble(p[1], lo)) return false;
		bytes[count] = (uint8_t)((hi << 4) | lo);
		mask[count] = true;
		++count;
		p += 2;
	}
	return count > 0;
}

void* Scan(const void* base, size_t size, const char* pattern)
{
	uint8_t bytes[256];
	bool mask[256];
	int count = 0;
	if (!ParsePattern(pattern, bytes, mask, count)) return nullptr;

	const uint8_t* buf = (const uint8_t*)base;
	if (size < (size_t)count) return nullptr;

	for (size_t i = 0; i + count <= size; ++i)
	{
		int j = 0;
		for (; j < count; ++j)
			if (mask[j] && buf[i + j] != bytes[j]) break;
		if (j == count) return (void*)(buf + i);
	}
	return nullptr;
}

const char* FindString(const void* base, size_t size, const char* literal)
{
	const size_t litLen = strlen(literal);
	if (litLen == 0 || size < litLen) return nullptr;
	const uint8_t* buf = (const uint8_t*)base;
	const uint8_t first = (uint8_t)literal[0];
	for (size_t i = 0; i + litLen <= size; ++i)
	{
		if (buf[i] != first) { continue; }
		if (memcmp(buf + i, literal, litLen) == 0) return (const char*)(buf + i);
	}
	return nullptr;
}

void* RipTarget(const uint8_t* insn)
{
	// Recognizes: 48/4C 8D <modrm=05> disp32  (lea rip-relative)
	//             E8/E9 disp32                (call/jmp)
	if ((insn[0] == 0xE8 || insn[0] == 0xE9))
	{
		int32_t disp = *(const int32_t*)(insn + 1);
		return (void*)(insn + 5 + disp);
	}
	if ((insn[0] == 0x48 || insn[0] == 0x4C) && insn[1] == 0x8D && (insn[2] & 0xC7) == 0x05)
	{
		int32_t disp = *(const int32_t*)(insn + 3);
		return (void*)(insn + 7 + disp);
	}
	return nullptr;
}

std::vector<uint8_t*> FindXrefs(const void* base, size_t size, const void* target, bool callsOnly)
{
	std::vector<uint8_t*> out;
	const uint8_t* buf = (const uint8_t*)base;
	for (size_t i = 0; i + 7 <= size; ++i)
	{
		const uint8_t* p = buf + i;
		bool isCall = (p[0] == 0xE8);
		bool isJmp = (p[0] == 0xE9);
		bool isLea = ((p[0] == 0x48 || p[0] == 0x4C) && p[1] == 0x8D && (p[2] & 0xC7) == 0x05);
		if (callsOnly && !isCall) continue;
		if (!isCall && !isJmp && !isLea) continue;
		if (RipTarget(p) == target)
			out.push_back((uint8_t*)p);
	}
	return out;
}

uint8_t* FunctionStart(uint8_t* addr, size_t maxBack)
{
	// Scan backwards: function start = after a run of 0xCC (or 0x00 padding) then prologue byte.
	for (size_t back = 1; back < maxBack; ++back)
	{
		uint8_t* p = addr - back;
		bool prevPad = (back > 0) && (*(p - 1) == 0xCC || *(p - 1) == 0x00 || *(p - 1) == 0x90);
		bool isPad = (*p == 0xCC || *p == 0x00 || *p == 0x90);
		if (prevPad && !isPad)
		{
			// accept only plausible first bytes (push/REX/sub/mov/int3-aligned jmp)
			uint8_t b = *p;
			if ((b >= 0x50 && b <= 0x5F) || b == 0x40 || b == 0x48 || b == 0x49 ||
				b == 0xE8 || b == 0xE9 || b == 0x55 || b == 0xF6 || b == 0xFF)
				return p;
		}
	}
	return nullptr;
}

static IMAGE_NT_HEADERS* NtHdr(uintptr_t base)
{
	IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
	IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
	return nt;
}

bool ModuleRange(const wchar_t* moduleName, uintptr_t& base, size_t& size)
{
	HMODULE h = GetModuleHandleW(moduleName);
	if (!h) return false;
	IMAGE_NT_HEADERS* nt = NtHdr((uintptr_t)h);
	if (!nt) return false;
	base = (uintptr_t)h;
	size = nt->OptionalHeader.SizeOfImage;
	return true;
}

bool MainModuleRange(uintptr_t& base, size_t& size)
{
	HMODULE h = GetModuleHandleW(nullptr);
	if (!h) return false;
	IMAGE_NT_HEADERS* nt = NtHdr((uintptr_t)h);
	if (!nt) return false;
	base = (uintptr_t)h;
	size = nt->OptionalHeader.SizeOfImage;
	return true;
}

std::vector<Section> ExecSections(uintptr_t base)
{
	std::vector<Section> out;
	IMAGE_NT_HEADERS* nt = NtHdr(base);
	if (!nt) return out;
	IMAGE_SECTION_HEADER* s = IMAGE_FIRST_SECTION(nt);
	for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++s)
	{
		if (s->Characteristics & IMAGE_SCN_MEM_EXECUTE)
			out.push_back({ (const char*)s->Name, (uint8_t*)base + s->VirtualAddress, s->Misc.VirtualSize });
	}
	return out;
}

} // namespace sig
