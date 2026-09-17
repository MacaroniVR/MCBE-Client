#include "durability_display.h"
#include <imgui.h>

DurabilityDisplay* g_durability_display = nullptr;

DurabilityDisplay::DurabilityDisplay()
    : Module("Durability Display", "Show held item durability (WIP)", 0) {
    g_durability_display = this;
    enabled = false;  // Off — needs item-component RE before it does anything
}

// Intentionally inert for now. Reading durability needs the item-stack component
// chain (ECS) which isn't mapped yet. Kept registered so the menu slot exists.
void DurabilityDisplay::OnRenderUI() {
    ImGui::Checkbox("Durability Display", &enabled);
    ImGui::TextDisabled("Not implemented yet (needs item RE)");
}
