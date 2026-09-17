#include "game_hooks.h"
#include "../memory/sigscan.h"
#include "../hooks/fov_capture.h"
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

        // Before the world is drawn, override the render origin with the free cam
        // position. The renderer draws the world from this origin, so the camera
        // is placed there directly — nothing recomputes it afterward.
        if (lvl) {
            SDK::LevelRendererPlayer* lrp = lvl->getLevelRendererPlayer();
            if (lrp) {
                // 0x654 is the render origin blocks/chunks draw from — the fix.
                Freecam::ApplyToOrigin(lrp->getRenderOrigin(), lrp->getViewTarget());
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
        if (FovCapture::Init())
            std::cout << "[+] GameHooks: FOV capture active\n";
        else
            std::cout << "[!] GameHooks: FOV capture failed (zoom disabled)\n";

        return true;
    }

    void Shutdown() { FovCapture::Shutdown(); }
}
