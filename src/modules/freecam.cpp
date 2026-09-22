#include "freecam.h"
#include <Windows.h>
#include <imgui.h>
#include <cmath>

Freecam* g_freecam = nullptr;

Freecam::Freecam()
    : Module("Freecam", "Detach the camera and fly freely", 0) {
    g_freecam = this;
}

void Freecam::OnEnable()  { posInitialized = false; }
void Freecam::OnDisable() { posInitialized = false; }

// Called from the renderLevel hook with the LevelRendererPlayer origin (0x654)
// and view target (0x660). We:
//   1. On first frame, snap freePos to the real origin.
//   2. Move freePos with WASD/Space/Shift.
//   3. Write freePos into 0x654/0x660 (moves entities, particles, chunk sort).
// The camera hook (CameraHook) separately shifts the render camera object so the
// BLOCKS follow too — together this is full freecam.
void Freecam::ApplyToOrigin(SDK::Vec3& cameraPos, SDK::Vec3& unused) {
    if (!g_freecam || !g_freecam->enabled) return;
    if (IsBadWritePtr(&cameraPos, sizeof(SDK::Vec3))) return;

    if (!g_freecam->posInitialized) {
        g_freecam->freePos = cameraPos;
        g_freecam->posInitialized = true;
    }

    float spd = g_freecam->speed;
    SDK::Vec3& p = g_freecam->freePos;

    bool focused = true;
    {
        HWND fg = GetForegroundWindow(); DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        focused = (pid == GetCurrentProcessId());
    }
    if (focused) {
        // World-axis movement for now (view-relative can come once blocks follow)
        if (GetAsyncKeyState('W') & 0x8000) p.z += spd;
        if (GetAsyncKeyState('S') & 0x8000) p.z -= spd;
        if (GetAsyncKeyState('A') & 0x8000) p.x -= spd;
        if (GetAsyncKeyState('D') & 0x8000) p.x += spd;
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) p.y += spd;
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) p.y -= spd;
    }

    // Write our free position into the render cameraPos
    cameraPos.x = p.x;
    cameraPos.y = p.y;
    cameraPos.z = p.z;
}

void Freecam::OnRenderUI() {
    ImGui::SliderFloat("Speed", &speed, 0.1f, 5.0f, "%.1f");
    if (enabled && posInitialized)
        ImGui::Text("Cam: %.1f, %.1f, %.1f", freePos.x, freePos.y, freePos.z);
}
