#pragma once
#include "module.h"

struct Vec3 {
    float x, y, z;
};

class Freecam : public Module {
public:
    float speed = 0.5f;
    Vec3 savedPos = { 0, 0, 0 };
    Vec3 camPos = { 0, 0, 0 };

    Freecam();

    void OnEnable() override;
    void OnDisable() override;
    void OnTick() override;
    void OnRenderUI() override;
};
