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

void Freecam::OnUpdatePlayer(SDK::CameraComponent* cam) {
    if (!g_freecam || !g_freecam->enabled || !cam) return;

    SDK::LocalPlayer* lp = SDK::GetLocalPlayer();

    // First frame: snap free cam to current cam pos, record body freeze point
    if (!g_freecam->posInitialized) {
        g_freecam->freePos = cam->cameraPos;
        if (lp) g_freecam->frozenPos = lp->getPos();
        g_freecam->posInitialized = true;
    }

    // Freeze the real body: pin position + kill velocity every tick
    if (lp) {
        lp->getPos() = g_freecam->frozenPos;
        lp->getVelocity() = { 0, 0, 0 };
    }

    // Compute yaw from camera quaternion for WASD-relative movement
    float qx = cam->lookAngles.x, qy = cam->lookAngles.y;
    float qz = cam->lookAngles.z, qw = cam->lookAngles.w;
    float fwdX = 2.0f * (qx * qz + qw * qy);
    float fwdZ = 1.0f - 2.0f * (qx * qx + qy * qy);
    float yaw = atan2f(fwdX, fwdZ);
    float s = sinf(yaw), c = cosf(yaw);

    float spd = g_freecam->speed;
    SDK::Vec3& p = g_freecam->freePos;

    if (GetAsyncKeyState('W') & 0x8000) { p.x += s * spd; p.z += c * spd; }
    if (GetAsyncKeyState('S') & 0x8000) { p.x -= s * spd; p.z -= c * spd; }
    if (GetAsyncKeyState('A') & 0x8000) { p.x -= c * spd; p.z += s * spd; }
    if (GetAsyncKeyState('D') & 0x8000) { p.x += c * spd; p.z -= s * spd; }
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) p.y += spd;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) p.y -= spd;

    // Fly the render camera
    cam->cameraPos = p;
}

void Freecam::OnRenderUI() {
    ImGui::SliderFloat("Speed", &speed, 0.1f, 5.0f, "%.1f");
    if (enabled && posInitialized) {
        ImGui::Text("Cam: %.1f, %.1f, %.1f", freePos.x, freePos.y, freePos.z);
    }
}
