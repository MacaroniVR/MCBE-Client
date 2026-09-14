#pragma once
#include "module.h"
#include "../mc/sdk.h"

class Freecam : public Module {
public:
    float speed = 0.6f;
    bool posInitialized = false;
    SDK::Vec3 freePos = { 0, 0, 0 };
    float yaw = 0.0f;   // updated from origin delta / kept from last movement

    Freecam();

    void OnEnable() override;
    void OnDisable() override;
    void OnRenderUI() override;

    // Called from renderLevel hook with a reference to the render origin.
    // Reads current origin on first frame, then overwrites it with freePos.
    static void ApplyToOrigin(SDK::Vec3& origin);
};

extern Freecam* g_freecam;
