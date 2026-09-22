#include "mem.h"

namespace Mem {

    uintptr_t GetModuleBase(const char* moduleName) {
        return reinterpret_cast<uintptr_t>(GetModuleHandleA(moduleName));
    }

    uintptr_t PatternScan(uintptr_t start, size_t size, const char* pattern, const char* mask) {
        size_t patternLen = strlen(mask);
        for (size_t i = 0; i < size - patternLen; i++) {
            bool found = true;
            for (size_t j = 0; j < patternLen; j++) {
                if (mask[j] != '?' && pattern[j] != *reinterpret_cast<char*>(start + i + j)) {
                    found = false;
                    break;
                }
            }
            if (found) return start + i;
        }
        return 0;
    }

    uintptr_t FindSig(const char* pattern, const char* mask) {
        HMODULE hMod = GetModuleHandleA("Minecraft.Windows.exe");
        if (!hMod) return 0;

        MODULEINFO modInfo;
        GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo));

        return PatternScan(
            reinterpret_cast<uintptr_t>(hMod),
            modInfo.SizeOfImage,
            pattern, mask
        );
    }

    void Patch(void* dst, const void* src, size_t size) {
        DWORD oldProtect;
        VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy(dst, src, size);
        VirtualProtect(dst, size, oldProtect, &oldProtect);
    }

    void Nop(void* dst, size_t size) {
        DWORD oldProtect;
        VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect);
        memset(dst, 0x90, size);
        VirtualProtect(dst, size, oldProtect, &oldProtect);
    }
}

namespace Mem {
    uintptr_t FindDMAAddy(uintptr_t base, const std::vector<unsigned int>& offsets) {
        uintptr_t addr = base;
        for (unsigned int off : offsets) {
            if (IsBadReadPtr(reinterpret_cast<void*>(addr), sizeof(uintptr_t)))
                return 0;
            addr = *reinterpret_cast<uintptr_t*>(addr);
            if (!addr) return 0;
            addr += off;
        }
        if (IsBadReadPtr(reinterpret_cast<void*>(addr), sizeof(uintptr_t)))
            return 0;
        return addr;
    }
}
