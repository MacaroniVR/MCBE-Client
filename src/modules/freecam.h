#pragma once
#include "module.h"
#include "../mc/sdk.h"

class Freecam : public Module {
public:
    float speed = 0.6f;
    bool posInitialized = false;
    SDK::Vec3 freePos = { 0, 0, 0 };   // flying camera position
    SDK::Vec3 offset  = { 0, 0, 0 };   // freePos - renderOrigin, applied to both vecs
    float yaw = 0.0f;                  // player yaw, for WASD direction

    Freecam();

    void OnEnable() override;
    void OnDisable() override;
    void OnRenderUI() override;

    // Called from the renderLevel hook every frame with references to BOTH camera
    // vectors in LevelRendererPlayer:
    //   renderOrigin (0x654) — blocks/chunks draw from here
    //   viewTarget   (0x660) — view direction reference
    // We shift both by the same delta so the whole view moves together and blocks follow.
    static void ApplyToOrigin(SDK::Vec3& renderOrigin, SDK::Vec3& viewTarget);
};

extern Freecam* g_freecam;
