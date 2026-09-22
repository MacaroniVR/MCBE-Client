#include "client_capture.h"
#include "../memory/sigscan.h"
#include <Windows.h>
#include <MinHook.h>
#include <iostream>

namespace ClientCapture {

    volatile uintptr_t g_clientInstance = 0;

    // 1.21.5X sig for ClientInstance::_updateScreenSizeVariables (this = ClientInstance).
    // Volatile stack displacements/sizes wildcarded so it matches across 1.21.5X sub-versions.
    static const char* SIG =
        "48 8B C4 48 89 58 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D A8 ? ? ? ? "
        "48 81 EC ? ? ? ? 0F 29 70 ? 0F 29 78 ? 44 0F 29 40 ? 44 0F 29 48 ? "
        "44 0F 29 90 ? ? ? ? 44 0F 29 A0";

    typedef void(__fastcall* fn_t)(void* thisPtr, void* a2);
    static fn_t oOriginal = nullptr;

    static void __fastcall hkUpdate(void* thisPtr, void* a2) {
        // this = ClientInstance. Validate it resolves a sane LevelRender chain before
        // trusting it (guards against a wrong sig match feeding garbage).
        if (thisPtr && !g_clientInstance) {
            uintptr_t ci = reinterpret_cast<uintptr_t>(thisPtr);
            if (!IsBadReadPtr((void*)(ci + 0xE8), 8)) {
                uintptr_t lr = *(uintptr_t*)(ci + 0xE8);
                if (lr > 0x10000 && !IsBadReadPtr((void*)(lr + 0x318), 8)) {
                    uintptr_t lrp = *(uintptr_t*)(lr + 0x318);
                    if (lrp > 0x10000 && !IsBadReadPtr((void*)(lrp + 0x6E4), 12)) {
                        g_clientInstance = ci;  // chain looks valid, accept
                    }
                }
            }
        }
        oOriginal(thisPtr, a2);
    }

    // Resolved 1.21.5 chain: ClientInstance +0xE8 -> LevelRender +0x318 -> LevelRendererPlayer
    uintptr_t GetLevelRendererPlayer() {
        uintptr_t ci = g_clientInstance;
        if (!ci) return 0;
        if (IsBadReadPtr((void*)(ci + 0xE8), 8)) return 0;
        uintptr_t levelRender = *(uintptr_t*)(ci + 0xE8);
        if (!levelRender || IsBadReadPtr((void*)(levelRender + 0x318), 8)) return 0;
        uintptr_t lrp = *(uintptr_t*)(levelRender + 0x318);
        if (!lrp || IsBadReadPtr((void*)(lrp + 0x6E4), 12)) return 0;
        return lrp;
    }

    bool Init() {
        uintptr_t fn = SigScan::Find(SIG);
        if (!fn) {
            std::cout << "[!] ClientCapture: sig not found\n";
            return false;
        }
        std::cout << "[+] ClientCapture: _updateScreenSizeVariables @ 0x" << std::hex << fn << std::dec << "\n";
        if (MH_CreateHook((void*)fn, hkUpdate, (void**)&oOriginal) != MH_OK) {
            std::cout << "[!] ClientCapture: create failed\n"; return false;
        }
        if (MH_EnableHook((void*)fn) != MH_OK) {
            std::cout << "[!] ClientCapture: enable failed\n"; return false;
        }
        std::cout << "[+] ClientCapture: active\n";
        return true;
    }

    void Shutdown() {}
}
