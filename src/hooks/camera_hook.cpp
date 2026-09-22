#include "camera_hook.h"
#include "../memory/sigscan.h"
#include "../modules/freecam.h"
#include "../mc/sdk.h"
#include <Windows.h>
#include <MinHook.h>
#include <iostream>

namespace CameraHook {

    // Prologue of FUN_1425f9800 (unique)
    static const char* SIG =
        "55 41 56 56 57 53 48 83 EC 60 48 8D 6C 24 60 48 C7 45 F8 FE FF FF FF 4C 89 C6 48 89 D7 49 89 CE";

    typedef void* (*BuildCamera_t)(void* result, void* p2, void* p3, void* p4);
    static BuildCamera_t oBuildCamera = nullptr;

    static void* __fastcall hkBuildCamera(void* result, void* p2, void* p3, void* p4) {
        // Let the game build the camera normally
        void* ret = oBuildCamera(result, p2, p3, p4);

        if (g_freecam && g_freecam->enabled) {
            uintptr_t* arr = reinterpret_cast<uintptr_t*>(result);
            uintptr_t cam = arr[0x15];
            if (cam && !IsBadWritePtr((void*)(cam + 0x28), 0x18)) {
                float* posX = reinterpret_cast<float*>(cam + 0x34);
                float* posY = reinterpret_cast<float*>(cam + 0x38);
                float* posZ = reinterpret_cast<float*>(cam + 0x3c);
                float* tgtX = reinterpret_cast<float*>(cam + 0x28);
                float* tgtY = reinterpret_cast<float*>(cam + 0x2c);
                float* tgtZ = reinterpret_cast<float*>(cam + 0x30);

                // Self-initialize from the real camera on the first frame so this
                // works standalone (doesn't depend on the renderLevel origin-write).
                if (!g_freecam->posInitialized) {
                    g_freecam->freePos.x = *posX;
                    g_freecam->freePos.y = *posY;
                    g_freecam->freePos.z = *posZ;
                    g_freecam->posInitialized = true;
                }

                float fx = g_freecam->freePos.x;
                float fy = g_freecam->freePos.y;
                float fz = g_freecam->freePos.z;

                // Shift target by same delta so look direction is preserved
                float dx = fx - *posX;
                float dy = fy - *posY;
                float dz = fz - *posZ;

                *posX = fx;  *posY = fy;  *posZ = fz;
                *tgtX += dx; *tgtY += dy; *tgtZ += dz;
            }
        }
        return ret;
    }

    bool Init() {
        // Offsets CONFIRMED from FUN_1425f9800 decompile on 1.21.51: the camera object
        // at result[0x15] gets pos at +0x34 (X/Y), +0x3c (Z) and target at +0x28/+0x30.
        // Overwriting them after the build makes BLOCKS render from freecam pos.
        uintptr_t fn = SigScan::Find(SIG);
        if (!fn) {
            std::cout << "[!] CameraHook: sig not found\n";
            return false;
        }
        std::cout << "[+] CameraHook: camera builder @ 0x" << std::hex << fn << std::dec << "\n";
        if (MH_CreateHook((void*)fn, hkBuildCamera, (void**)&oBuildCamera) != MH_OK) {
            std::cout << "[!] CameraHook: create failed\n";
            return false;
        }
        if (MH_EnableHook((void*)fn) != MH_OK) {
            std::cout << "[!] CameraHook: enable failed\n";
            return false;
        }
        std::cout << "[+] CameraHook: active (full freecam)\n";
        return true;
    }

    void Shutdown() {}
}
