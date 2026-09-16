#include "freecam.h"
#include <Windows.h>
#include <imgui.h>
#include <cmath>
#include <iostream>

Freecam* g_freecam = nullptr;

Freecam::Freecam()
    : Module("Freecam", "Detach the camera and fly freely", VK_F4) {
    g_freecam = this;
}

void Freecam::OnEnable() {
    posInitialized = false;
    std::cout << "[Freecam] Enabled\n";
}

void Freecam::OnDisable() {
    posInitialized = false;
    std::cout << "[Freecam] Disabled\n";
}

// renderOrigin = 0x654 (what blocks/chunks render from) — the value that was never
// being moved before. viewTarget = 0x660 (Latite's old "origin", view direction ref).
// We move BOTH by the same delta so the world and the look vector stay consistent.
void Freecam::ApplyToOrigin(SDK::Vec3& renderOrigin, SDK::Vec3& viewTarget) {
    if (!g_freecam || !g_freecam->enabled) return;

    // First frame after enabling: snap free position to the real render origin
    if (!g_freecam->posInitialized) {
        g_freecam->freePos = renderOrigin;
        g_freecam->posInitialized = true;
    }

    // Player yaw for WASD-relative movement (from LocalPlayer rotation)
    SDK::LocalPlayer* lp = SDK::GetLocalPlayer();
    if (lp) g_freecam->yaw = lp->getRot().y;

    float yaw = g_freecam->yaw * 3.14159265f / 180.0f;
    float s = sinf(yaw), c = cosf(yaw);

    float spd = g_freecam->speed;
    SDK::Vec3& p = g_freecam->freePos;

    // Bedrock: yaw 0 faces +Z, forward = (-sin, 0, cos)
    if (GetAsyncKeyState('W') & 0x8000) { p.x -= s * spd; p.z += c * spd; }
    if (GetAsyncKeyState('S') & 0x8000) { p.x += s * spd; p.z -= c * spd; }
    if (GetAsyncKeyState('A') & 0x8000) { p.x -= c * spd; p.z -= s * spd; }
    if (GetAsyncKeyState('D') & 0x8000) { p.x += c * spd; p.z += s * spd; }
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) p.y += spd;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) p.y -= spd;

    // Delta from the real render origin to our free position
    SDK::Vec3 delta = {
        p.x - renderOrigin.x,
        p.y - renderOrigin.y,
        p.z - renderOrigin.z
    };

    // Shift both camera vectors by the same delta. renderOrigin moving is what
    // finally makes the blocks/chunks detach; viewTarget moving keeps the look
    // direction pointing the same way relative to the camera.
    renderOrigin.x += delta.x; renderOrigin.y += delta.y; renderOrigin.z += delta.z;
    viewTarget.x   += delta.x; viewTarget.y   += delta.y; viewTarget.z   += delta.z;
}

void Freecam::OnRenderUI() {
    ImGui::SliderFloat("Speed", &speed, 0.1f, 5.0f, "%.1f");
    if (enabled && posInitialized) {
        ImGui::Text("Cam: %.1f, %.1f, %.1f", freePos.x, freePos.y, freePos.z);
    }
}
