#include "fps_counter.h"
#include <imgui.h>

FpsCounter* g_fps_counter = nullptr;

FpsCounter::FpsCounter()
    : Module("FPS Counter", "Show frames per second on screen", 0) {
    g_fps_counter = this;
}

// Draws a small FPS readout top-left when enabled. Uses ImGui's own frame timing.
void FpsCounter::Draw() {
    if (!g_fps_counter || !g_fps_counter->enabled) return;

    ImGuiIO& io = ImGui::GetIO();
    g_fps_counter->fps = io.Framerate;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f FPS", io.Framerate);

    // Shadow + text, top-left
    ImVec2 pos(12, 12);
    dl->AddText(ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 200), buf);
    dl->AddText(pos, IM_COL32(100, 180, 255, 255), buf);
}

void FpsCounter::OnRenderUI() {
    ImGui::Text("Current: %.0f FPS", fps);
}
