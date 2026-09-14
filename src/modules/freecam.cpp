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
    posInitialized = false; // grab current cam pos on next update
    std::cout << "[Freecam] Enabled\n";
}

void Freecam::OnDisable() {
    posInitialized = false;
    std::cout << "[Freecam] Disabled\n";
}

// Called from GenericHooks::hkUpdatePlayer each camera tick.
// cam->cameraPos is the position the renderer will use this frame.
void Freecam::OnUpdatePlayer(SDK::CameraComponent* cam) {
    if (!g_freecam || !g_freecam->enabled || !cam) return;

    // On first frame after enabling, snap our free position to the real camera
    if (!g_freecam->posInitialized) {
        g_freecam->freePos = cam->cameraPos;
        g_freecam->posInitialized = true;
    }

    // Derive yaw from the camera quaternion's look direction.
    // lookAngles is a quaternion (x,y,z,w). We only need a yaw for WASD.
    // Convert quaternion -> yaw (around Y).
    float qx = cam->lookAngles.x;
    float qy = cam->lookAngles.y;
    float qz = cam->lookAngles.z;
    float qw = cam->lookAngles.w;

    // Forward vector from quaternion
    float fwdX = 2.0f * (qx * qz + qw * qy);
    float fwdZ = 1.0f - 2.0f * (qx * qx + qy * qy);
    float yaw = atan2f(fwdX, fwdZ);

    float sinYaw = sinf(yaw);
    float cosYaw = cosf(yaw);

    float spd = g_freecam->speed;
    SDK::Vec3& p = g_freecam->freePos;

    // WASD relative to camera yaw
    if (GetAsyncKeyState('W') & 0x8000) { p.x += sinYaw * spd; p.z += cosYaw * spd; }
    if (GetAsyncKeyState('S') & 0x8000) { p.x -= sinYaw * spd; p.z -= cosYaw * spd; }
    if (GetAsyncKeyState('A') & 0x8000) { p.x -= cosYaw * spd; p.z += sinYaw * spd; }
    if (GetAsyncKeyState('D') & 0x8000) { p.x += cosYaw * spd; p.z -= sinYaw * spd; }
    if (GetAsyncKeyState(VK_SPACE) & 0x8000)   p.y += spd;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   p.y -= spd;

    // Overwrite the render camera position with our free position
    cam->cameraPos = p;
}

void Freecam::OnRenderUI() {
    ImGui::SliderFloat("Speed", &speed, 0.1f, 5.0f, "%.1f");
    if (enabled && posInitialized) {
        ImGui::Text("Cam: %.1f, %.1f, %.1f", freePos.x, freePos.y, freePos.z);
    }
}
