#include "game_hooks.h"
#include "../memory/sigscan.h"
#include "../modules/freecam.h"
#include <Windows.h>
#include <MinHook.h>
#include <iostream>

namespace GameHooks {

    // UpdatePlayerFromCameraSystemUtil::_updatePlayer
    // arg1 (rcx) = CameraComponent*
    using UpdatePlayerFn = void(__fastcall*)(SDK::CameraComponent*, void*, void*);
    static UpdatePlayerFn oUpdatePlayer = nullptr;

    static const char* SIG_UPDATE_PLAYER =
        "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC ? ? ? ? 44 0F 29 94 24 ? ? ? ? 44 0F 29 8C 24 ? ? ? ? 44 0F 29 84 24 ? ? ? ? 0F 29 BC 24 ? ? ? ? 0F 29 B4 24 ? ? ? ? 4C 89 C6";

    static void __fastcall hkUpdatePlayer(SDK::CameraComponent* cam, void* a, void* b) {
        // Run the original first so the game sets up the camera normally
        oUpdatePlayer(cam, a, b);

        // Then let freecam override the camera position if enabled
        Freecam::OnUpdatePlayer(cam);
    }

    bool Init() {
        uintptr_t addr = SigScan::Find(SIG_UPDATE_PLAYER);
        if (!addr) {
            std::cout << "[!] GameHooks: _updatePlayer sig not found\n";
            return false;
        }
        std::cout << "[+] GameHooks: _updatePlayer @ 0x" << std::hex << addr << std::dec << "\n";

        if (MH_CreateHook((void*)addr, hkUpdatePlayer, (void**)&oUpdatePlayer) != MH_OK) {
            std::cout << "[!] GameHooks: failed to create _updatePlayer hook\n";
            return false;
        }
        if (MH_EnableHook((void*)addr) != MH_OK) {
            std::cout << "[!] GameHooks: failed to enable _updatePlayer hook\n";
            return false;
        }
        std::cout << "[+] GameHooks: _updatePlayer hooked\n";
        return true;
    }

    void Shutdown() {
        // MinHook disables all in the main hook shutdown
    }
}
