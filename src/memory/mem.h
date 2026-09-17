#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>
#include <string>
#include <Psapi.h>

namespace Mem {
    uintptr_t GetModuleBase(const char* moduleName);
    uintptr_t PatternScan(uintptr_t start, size_t size, const char* pattern, const char* mask);
    uintptr_t FindSig(const char* pattern, const char* mask);
    void Patch(void* dst, const void* src, size_t size);
    void Nop(void* dst, size_t size);

    // Walks a multi-level pointer chain: deref base, add offset, repeat.
    // Returns 0 if any link is unreadable (so callers can null-check instead of crashing).
    uintptr_t FindDMAAddy(uintptr_t base, const std::vector<unsigned int>& offsets);

    template<typename T>
    T Read(uintptr_t addr) {
        if (IsBadReadPtr(reinterpret_cast<void*>(addr), sizeof(T)))
            return T{};
        return *reinterpret_cast<T*>(addr);
    }

    template<typename T>
    void Write(uintptr_t addr, T value) {
        DWORD oldProtect;
        VirtualProtect(reinterpret_cast<void*>(addr), sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect);
        *reinterpret_cast<T*>(addr) = value;
        VirtualProtect(reinterpret_cast<void*>(addr), sizeof(T), oldProtect, &oldProtect);
    }
}
