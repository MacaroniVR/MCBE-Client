#include "game_hooks.h"
#include "../memory/sigscan.h"
#include "../modules/freecam.h"
#include "../mc/sdk.h"
#include <Windows.h>
#include <MinHook.h>
#include <iostream>

namespace GameHooks {

    using UpdatePlayerFn = void(__fastcall*)(SDK::CameraComponent*, void*, void*);
    static UpdatePlayerFn oUpdatePlayer = nullptr;

    static const char* SIG_UPDATE_PLAYER =
        "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC ? ? ? ? 44 0F 29 94 24 ? ? ? ? 44 0F 29 8C 24 ? ? ? ? 44 0F 29 84 24 ? ? ? ? 0F 29 BC 24 ? ? ? ? 0F 29 B4 24 ? ? ? ? 4C 89 C6";

    static void __fastcall hkUpdatePlayer(SDK::CameraComponent* cam, void* a, void* b) {
        // Latite pattern: modify the camera field BEFORE the original runs so the
        // original propagates our value through the render pipeline, then restore
        // afterward so nothing downstream fights it (prevents jitter).

        if (Freecam::WantsOverride(cam)) {
            SDK::Vec3 orig = cam->cameraPos;
            cam->cameraPos = Freecam::GetOverridePos(cam);
            oUpdatePlayer(cam, a, b);
            cam->cameraPos = orig;
            return;
        }

        oUpdatePlayer(cam, a, b);
    }

    bool Init() {
        uintptr_t addr = SigScan::Find(SIG_UPDATE_PLAYER);
        if (!addr) {
            std::cout << "[!] GameHooks: _updatePlayer sig not found\n";
            return false;
        }
        std::cout << "[+] GameHooks: _updatePlayer @ 0x" << std::hex << addr << std::dec << "\n";

        if (MH_CreateHook((void*)addr, hkUpdatePlayer, (void**)&oUpdatePlayer) != MH_OK) {
            std::cout << "[!] GameHooks: create hook failed\n";
            return false;
        }
        if (MH_EnableHook((void*)addr) != MH_OK) {
            std::cout << "[!] GameHooks: enable hook failed\n";
            return false;
        }
        std::cout << "[+] GameHooks: _updatePlayer hooked\n";
        return true;
    }

    void Shutdown() {}
}
