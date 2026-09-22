#include "game_hooks.h"
#include "../memory/sigscan.h"
#include "../hooks/fov_capture.h"
#include "../hooks/camera_hook.h"
#include "../hooks/client_capture.h"
#include "../modules/freecam.h"
#include "../modules/auto_sprint.h"
#include "../modules/durability_display.h"
#include "../modules/zoom.h"
#include "../mc/sdk.h"
#include <Windows.h>
#include <MinHook.h>
#include <iostream>

namespace GameHooks {

    // LevelRenderer::renderLevel(LevelRenderer*, ScreenContext*, void*)
    using RenderLevelFn = void(__fastcall*)(SDK::LevelRenderer*, void*, void*);
    static RenderLevelFn oRenderLevel = nullptr;

    // renderLevel sig resolves to a CALL — deref(1) to get the function.
    // We use the same pattern the sig test proved hits: "E8 ? ? ? ? 45 31 E4 48 83 BE"
    static const char* SIG_RENDER_LEVEL = "E8 ? ? ? ? 45 31 E4 48 83 BE";

    static void __fastcall hkRenderLevel(SDK::LevelRenderer* lvl, void* scn, void* unk) {
        // Auto sprint: synthesize Ctrl via SendInput while W is held (no ECS poking)
        AutoSprint::Tick();

        // Zoom: scale FOV setting via DMA pointer chain while key held
        Zoom::Tick();

        // Freecam: use the CORRECT chain (ClientInstance +0xE8 -> +0x318 -> LevelRendererPlayer),
        // captured by ClientCapture, then write cameraPos at +0x6E4. The old renderLevel-arg
        // +0x468 path pointed at a different struct (only moved entities).
        {
            uintptr_t lrp = ClientCapture::GetLevelRendererPlayer();
            if (lrp) {
                SDK::Vec3& camPos = *reinterpret_cast<SDK::Vec3*>(lrp + 0x6E4);
                Freecam::ApplyToOrigin(camPos, camPos);  // single cameraPos Vec3
            }
        }
        oRenderLevel(lvl, scn, unk);
    }

    static uintptr_t ResolveCall(uintptr_t sigAddr) {
        // sig starts at the E8 (call rel32). target = addr + 5 + disp32
        int32_t disp = *reinterpret_cast<int32_t*>(sigAddr + 1);
        return sigAddr + 5 + disp;
    }

    bool Init() {
        uintptr_t sig = SigScan::Find(SIG_RENDER_LEVEL);
        if (!sig) {
            std::cout << "[!] GameHooks: renderLevel sig not found\n";
            return false;
        }
        uintptr_t fn = ResolveCall(sig);
        std::cout << "[+] GameHooks: renderLevel @ 0x" << std::hex << fn << std::dec << "\n";

        if (MH_CreateHook((void*)fn, hkRenderLevel, (void**)&oRenderLevel) != MH_OK) {
            std::cout << "[!] GameHooks: create renderLevel hook failed\n";
            return false;
        }
        if (MH_EnableHook((void*)fn) != MH_OK) {
            std::cout << "[!] GameHooks: enable renderLevel hook failed\n";
            return false;
        }
        std::cout << "[+] GameHooks: renderLevel hooked\n";

        // Install the FOV-write capture (inline hook) so Zoom can find the FOV struct
        if (ClientCapture::Init())
            std::cout << "[+] GameHooks: client capture active\n";
        else
            std::cout << "[!] GameHooks: client capture failed (freecam disabled)\n";

        if (CameraHook::Init())
            std::cout << "[+] GameHooks: camera hook active\n";
        else
            std::cout << "[!] GameHooks: camera hook failed (freecam blocks wont move)\n";

        if (FovCapture::Init())
            std::cout << "[+] GameHooks: FOV capture active\n";
        else
            std::cout << "[!] GameHooks: FOV capture failed (zoom disabled)\n";

        return true;
    }

    void Shutdown() { FovCapture::Shutdown(); CameraHook::Shutdown(); ClientCapture::Shutdown(); }
}
