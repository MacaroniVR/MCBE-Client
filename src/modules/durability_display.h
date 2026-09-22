#pragma once
#include "module.h"

class DurabilityDisplay : public Module {
public:
    DurabilityDisplay();
    void OnRenderUI() override;
};

extern DurabilityDisplay* g_durability_display;
