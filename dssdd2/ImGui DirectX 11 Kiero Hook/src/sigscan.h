#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <vector>

// ── Pattern scanner ──
// Signature syntax: "48 89 5C 24 ? 57" ('?' = wildcard nibble pair)

namespace sig {

// Search a raw memory block. Returns address of first match, or nullptr.
void* Scan(const void* base, size_t size, const char* pattern);

// Find a string anywhere in [base, base+size). Returns pointer to it, or nullptr.
const char* FindString(const void* base, size_t size, const char* literal);

// Find RIP-relative xrefs (E8 call / E9 jmp / 4C 8D / 48 8D lea) pointing at `target`
// within [base, base+size). Returns absolute addresses of the referencing instructions.
std::vector<uint8_t*> FindXrefs(const void* base, size_t size, const void* target, bool callsOnly);

// Resolve `lea reg, [rip+disp]` / return the referenced absolute address at insn, or nullptr.
void* RipTarget(const uint8_t* insn);

// Locate module base+size for the main exe and for a given name.
bool ModuleRange(const wchar_t* moduleName, uintptr_t& base, size_t& size);
bool MainModuleRange(uintptr_t& base, size_t& size);

// Walk backward from `addr` to the enclosing function start (0xCC padding + prologue).
uint8_t* FunctionStart(uint8_t* addr, size_t maxBack = 0x2000);

// Image section helpers for exec-only scanning
struct Section { const char* name; uint8_t* addr; size_t size; };
std::vector<Section> ExecSections(uintptr_t base);

} // namespace sig
