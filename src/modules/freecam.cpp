#include "freecam.h"
#include "../memory/mem.h"
#include <imgui.h>
#include <Windows.h>
#include <cmath>
#include <iostream>

// ============================================================
// OFFSET CONFIGURATION — update these for your game version
// ============================================================
// These need to be found via reverse engineering for v1.21.45.
// Common approach: sig scan for ClientInstance pointer, then
// walk the offset chain to LocalPlayer -> position.
//
// Example offset chains (these are PLACEHOLDERS):
//   ClientInstance  = sig scan result
//   LocalPlayer     = ClientInstance + 0x???
//   PlayerPos       = LocalPlayer + 0x???
//
// You can find these using Cheat Engine or x64dbg.
// ============================================================

static constexpr uintptr_t OFFSET_LOCAL_PLAYER = 0x0;  // TODO: fill in
static constexpr uintptr_t OFFSET_PLAYER_POS   = 0x0;  // TODO: fill in

Freecam::Freecam()
    : Module("Freecam", "Fly the camera freely", VK_F4) {}

void Freecam::OnEnable() {
    std::cout << "[Freecam] Enabled\n";

    // TODO: Once offsets are set, save player position here
    // uintptr_t localPlayer = getLocalPlayer();
    // if (localPlayer) {
    //     savedPos = Mem::Read<Vec3>(localPlayer + OFFSET_PLAYER_POS);
    //     camPos = savedPos;
    // }
}

void Freecam::OnDisable() {
    std::cout << "[Freecam] Disabled\n";

    // TODO: Restore player to saved position
    // uintptr_t localPlayer = getLocalPlayer();
    // if (localPlayer) {
    //     Mem::Write<Vec3>(localPlayer + OFFSET_PLAYER_POS, savedPos);
    // }
}

void Freecam::OnTick() {
    if (!enabled) return;

    // Camera movement with WASD + Space/Shift
    float dx = 0, dy = 0, dz = 0;

    if (GetAsyncKeyState('W') & 0x8000) dz += speed;
    if (GetAsyncKeyState('S') & 0x8000) dz -= speed;
    if (GetAsyncKeyState('A') & 0x8000) dx -= speed;
    if (GetAsyncKeyState('D') & 0x8000) dx += speed;
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) dy += speed;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) dy -= speed;

    camPos.x += dx;
    camPos.y += dy;
    camPos.z += dz;

    // TODO: Write camPos to the player/camera position in memory
    // uintptr_t localPlayer = getLocalPlayer();
    // if (localPlayer) {
    //     Mem::Write<Vec3>(localPlayer + OFFSET_PLAYER_POS, camPos);
    // }
}

void Freecam::OnRenderUI() {
    ImGui::SliderFloat("Speed", &speed, 0.1f, 5.0f, "%.1f");
    if (enabled) {
        ImGui::Text("Cam: %.1f, %.1f, %.1f", camPos.x, camPos.y, camPos.z);
    }
}
