#pragma once
#include "module.h"
#include "../mc/sdk.h"

class Freecam : public Module {
public:
    float speed = 0.6f;
    bool posInitialized = false;
    SDK::Vec3 freePos = { 0, 0, 0 };   // flying camera position
    SDK::Vec3 frozenPos = { 0, 0, 0 };  // where the real body is pinned

    Freecam();

    void OnEnable() override;
    void OnDisable() override;
    void OnRenderUI() override;

    // Called from the _updatePlayer hook each camera tick
    static void OnUpdatePlayer(SDK::CameraComponent* cam);
};

extern Freecam* g_freecam;
