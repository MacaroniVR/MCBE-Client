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

void Freecam::ApplyToOrigin(SDK::Vec3& origin) {
    if (!g_freecam || !g_freecam->enabled) return;

    // Snap to the real render origin on first frame
    if (!g_freecam->posInitialized) {
        g_freecam->freePos = origin;
        g_freecam->posInitialized = true;
    }

    // Use the local player's yaw for WASD direction
    float yawDeg = g_freecam->yaw;
    SDK::LocalPlayer* lp = SDK::GetLocalPlayer();
    if (lp) {
        // getRot returns Vec2 {x=pitch, y=yaw} in degrees
        yawDeg = lp->getRot().y;
        g_freecam->yaw = yawDeg;
    }

    float yaw = yawDeg * 3.14159265f / 180.0f;
    float s = sinf(yaw), c = cosf(yaw);

    float spd = g_freecam->speed;
    SDK::Vec3& p = g_freecam->freePos;

    // In Bedrock: yaw 0 faces +Z. forward = (-sin, 0, cos)
    if (GetAsyncKeyState('W') & 0x8000) { p.x -= s * spd; p.z += c * spd; }
    if (GetAsyncKeyState('S') & 0x8000) { p.x += s * spd; p.z -= c * spd; }
    if (GetAsyncKeyState('A') & 0x8000) { p.x -= c * spd; p.z -= s * spd; }
    if (GetAsyncKeyState('D') & 0x8000) { p.x += c * spd; p.z += s * spd; }
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) p.y += spd;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) p.y -= spd;

    // Overwrite the render origin — world draws from here
    origin = p;
}

void Freecam::OnRenderUI() {
    ImGui::SliderFloat("Speed", &speed, 0.1f, 5.0f, "%.1f");
    if (enabled && posInitialized) {
        ImGui::Text("Cam: %.1f, %.1f, %.1f", freePos.x, freePos.y, freePos.z);
    }
}
