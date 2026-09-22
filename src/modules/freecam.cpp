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
void Freecam::ApplyToOrigin(SDK::Vec3& renderOrigin, SDK::Vec3& viewTarget) {
    if (!g_freecam || !g_freecam->enabled) return;
    if (IsBadWritePtr(&renderOrigin, sizeof(SDK::Vec3)) ||
        IsBadWritePtr(&viewTarget, sizeof(SDK::Vec3))) return;

    if (!g_freecam->posInitialized) {
        g_freecam->freePos = renderOrigin;
        g_freecam->posInitialized = true;
    }

    // Forward from camera facing (target - origin), so WASD is look-relative
    float fwdX = viewTarget.x - renderOrigin.x;
    float fwdZ = viewTarget.z - renderOrigin.z;
    float len = sqrtf(fwdX * fwdX + fwdZ * fwdZ);
    if (len > 0.0001f) { fwdX /= len; fwdZ /= len; }
    else { fwdX = 0.0f; fwdZ = 1.0f; }
    float rightX = -fwdZ, rightZ = fwdX;

    float spd = g_freecam->speed;
    SDK::Vec3& p = g_freecam->freePos;

    bool focused = true;
    {
        HWND fg = GetForegroundWindow(); DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        focused = (pid == GetCurrentProcessId());
    }
    if (focused) {
        if (GetAsyncKeyState('W') & 0x8000) { p.x += fwdX * spd;   p.z += fwdZ * spd; }
        if (GetAsyncKeyState('S') & 0x8000) { p.x -= fwdX * spd;   p.z -= fwdZ * spd; }
        if (GetAsyncKeyState('A') & 0x8000) { p.x -= rightX * spd; p.z -= rightZ * spd; }
        if (GetAsyncKeyState('D') & 0x8000) { p.x += rightX * spd; p.z += rightZ * spd; }
        if (GetAsyncKeyState(VK_SPACE) & 0x8000) p.y += spd;
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) p.y -= spd;
    }

    // Shift both vectors by the delta from real origin to freePos
    float dx = p.x - renderOrigin.x;
    float dy = p.y - renderOrigin.y;
    float dz = p.z - renderOrigin.z;
    renderOrigin.x += dx; renderOrigin.y += dy; renderOrigin.z += dz;
    viewTarget.x   += dx; viewTarget.y   += dy; viewTarget.z   += dz;
}

void Freecam::OnRenderUI() {
    ImGui::SliderFloat("Speed", &speed, 0.1f, 5.0f, "%.1f");
    if (enabled && posInitialized)
        ImGui::Text("Cam: %.1f, %.1f, %.1f", freePos.x, freePos.y, freePos.z);
}
