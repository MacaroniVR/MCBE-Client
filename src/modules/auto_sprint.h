#pragma once
#include "module.h"

class AutoSprint : public Module {
public:
    AutoSprint();
    void OnRenderUI() override;

    // Called every frame from the render hook. Synthesizes the sprint key
    // (Ctrl) while W is held and the game window is focused.
    static void Tick();
};

extern AutoSprint* g_auto_sprint;
