#include "freecam.h"
#include <Windows.h>
#include <imgui.h>
#include <cmath>
#include <iostream>

Freecam* g_freecam = nullptr;

Freecam::Freecam()
    : Module("Freecam", "Detach the camera and fly freely", 0) {
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

// SAFE DIAGNOSTIC VERSION:
// - No GetLocalPlayer() call (that map-walk was the shakiest code; ruling it out)
// - Guards every access with IsBadReadPtr/IsBadWritePtr
// - Prints the origin values ONCE so we can confirm 0x654 holds real coordinates
void Freecam::ApplyToOrigin(SDK::Vec3& renderOrigin, SDK::Vec3& viewTarget) {
    if (!g_freecam || !g_freecam->enabled) return;

    // Guard: make sure both vectors are actually readable/writable before touching them
    if (IsBadWritePtr(&renderOrigin, sizeof(SDK::Vec3)) ||
        IsBadWritePtr(&viewTarget, sizeof(SDK::Vec3))) {
        std::cout << "[Freecam] Bad vector pointer — aborting write\n";
        return;
    }

    // First frame: snap + print what's actually at 0x654 and 0x660
    if (!g_freecam->posInitialized) {
        g_freecam->freePos = renderOrigin;
        g_freecam->posInitialized = true;
        std::cout << "[Freecam] renderOrigin(0x654) = "
                  << renderOrigin.x << ", " << renderOrigin.y << ", " << renderOrigin.z << "\n";
        std::cout << "[Freecam] viewTarget(0x660)   = "
                  << viewTarget.x << ", " << viewTarget.y << ", " << viewTarget.z << "\n";
    }

    // World-aligned movement (no yaw yet — ruling out the LocalPlayer chain)
    float spd = g_freecam->speed;
    SDK::Vec3& p = g_freecam->freePos;

    if (GetAsyncKeyState('W') & 0x8000) p.z += spd;
    if (GetAsyncKeyState('S') & 0x8000) p.z -= spd;
    if (GetAsyncKeyState('A') & 0x8000) p.x -= spd;
    if (GetAsyncKeyState('D') & 0x8000) p.x += spd;
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) p.y += spd;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) p.y -= spd;

    SDK::Vec3 delta = {
        p.x - renderOrigin.x,
        p.y - renderOrigin.y,
        p.z - renderOrigin.z
    };

    renderOrigin.x += delta.x; renderOrigin.y += delta.y; renderOrigin.z += delta.z;
    viewTarget.x   += delta.x; viewTarget.y   += delta.y; viewTarget.z   += delta.z;
}

void Freecam::OnRenderUI() {
    ImGui::SliderFloat("Speed", &speed, 0.1f, 5.0f, "%.1f");
    if (enabled && posInitialized) {
        ImGui::Text("Cam: %.1f, %.1f, %.1f", freePos.x, freePos.y, freePos.z);
    }
}
