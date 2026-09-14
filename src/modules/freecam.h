#pragma once
#include "module.h"

// Minimal SDK types matching the game's memory layout
namespace SDK {
    struct Vec2 { float x, y; };
    struct Vec3 { float x, y, z; };
    struct Vec4 { float x, y, z, w; };

    // CameraComponent — layout from Latite (verified for this build family)
    class CameraComponent {
    public:
        char pad_0000[0x30];    // 0x0000
        Vec4 lookAngles;        // 0x0030  (quaternion)
        Vec3 cameraPos;         // 0x0040
        Vec2 fov;               // 0x004C
        float nearClip;         // 0x0054
        float farClip;          // 0x0058
    };
}

class Freecam : public Module {
public:
    float speed = 0.6f;
    bool posInitialized = false;
    SDK::Vec3 freePos = { 0, 0, 0 };

    Freecam();

    void OnEnable() override;
    void OnDisable() override;
    void OnRenderUI() override;

    // Called from the _updatePlayer hook every camera update
    static void OnUpdatePlayer(SDK::CameraComponent* cam);
};

// Global instance pointer so the hook can reach the module state
extern Freecam* g_freecam;
