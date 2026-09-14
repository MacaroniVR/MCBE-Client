#pragma once
#include "module.h"
#include "../mc/sdk.h"

class Freecam : public Module {
public:
    float speed = 0.6f;
    bool posInitialized = false;
    SDK::Vec3 freePos = { 0, 0, 0 };

    Freecam();

    void OnEnable() override;
    void OnDisable() override;
    void OnRenderUI() override;

    // Hook queries — called from _updatePlayer hook
    static bool WantsOverride(SDK::CameraComponent* cam);
    static SDK::Vec3 GetOverridePos(SDK::CameraComponent* cam);
};

extern Freecam* g_freecam;
