#pragma once
#include "module.h"

class FpsCounter : public Module {
public:
    float fps = 0.0f;
    FpsCounter();
    void OnRenderUI() override;

    // Called every frame from the Present/overlay path to update + optionally draw.
    static void Draw();
};

extern FpsCounter* g_fps_counter;
