#include "zoom.h"
#include "../hooks/fov_capture.h"
#include <Windows.h>
#include <imgui.h>
#include <cmath>

Zoom* g_zoom = nullptr;

Zoom::Zoom()
    : Module("Zoom", "Hold to zoom (scales FOV)", VK_C) {
    g_zoom = this;
}

void Zoom::OnDisable() {
    // Don't leave the game zoomed if toggled off mid-zoom
    if (active) {
        float* fov = GetFovPtr();
        if (fov) *fov = savedFov;
        active = false;
    }
}

float* Zoom::GetFovPtr() {
    uintptr_t s = FovCapture::g_fovStruct;
    if (!s) return nullptr;
    if (IsBadWritePtr(reinterpret_cast<void*>(s + 0x18), sizeof(float))) return nullptr;
    return reinterpret_cast<float*>(s + 0x18);
}

static void stepToward(float* v, float target, float t) {
    if (!v) return;
    *v += (target - *v) * t;
    if (fabsf(target - *v) < 0.05f) *v = target;
}

void Zoom::Tick() {
    if (!g_zoom || !g_zoom->enabled) return;

    // Only act when Minecraft is focused
    HWND fg = GetForegroundWindow();
    DWORD pid = 0; GetWindowThreadProcessId(fg, &pid);
    if (pid != GetCurrentProcessId()) return;

    float* fov = g_zoom->GetFovPtr();
    if (!fov) return;   // struct not captured yet -> safe no-op

    bool held = (GetAsyncKeyState(g_zoom->keybind) & 0x8000) != 0;

    if (held) {
        if (!g_zoom->active) {
            g_zoom->savedFov = *fov;   // remember the player's real FOV
            g_zoom->active = true;
        }
        stepToward(fov, g_zoom->zoomFov, g_zoom->lerpTime);
    } else if (g_zoom->active) {
        stepToward(fov, g_zoom->savedFov, g_zoom->lerpTime);
        if (fabsf(*fov - g_zoom->savedFov) < 0.05f) {
            *fov = g_zoom->savedFov;
            g_zoom->active = false;
        }
    }
}

void Zoom::OnRenderUI() {
    ImGui::Checkbox("Zoom", &enabled);
    ImGui::SliderFloat("Zoom FOV", &zoomFov, 5.0f, 60.0f, "%.0f");
    ImGui::SliderFloat("Smooth", &lerpTime, 0.05f, 1.0f, "%.2f");
    ImGui::TextDisabled("Hold C to zoom");
    if (!FovCapture::g_fovStruct)
        ImGui::TextDisabled("FOV not captured yet - change FOV once in settings");
    else {
        float* f = GetFovPtr();
        if (f) ImGui::Text("FOV: %.1f", *f);
    }
}
