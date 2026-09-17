#pragma once
#include <cstdint>

// Hooks the per-frame camera builder (FUN_1425f9800). After the game builds the
// render camera object, we overwrite its position/view-target so the BLOCK view
// matrix (built downstream from this camera) draws the world from the freecam
// position. This is the piece that moving 0x654 alone couldn't reach — blocks
// use the camera object here, not the LevelRendererPlayer origin.
//
// Camera builder returns its result in param_1 (RCX). The actual camera object
// is at param_1[0x15] (byte offset 0xA8). Inside that object:
//   +0x34 = position Vec3 (x,y,z)
//   +0x28 = view target Vec3 (x,y,z)   [+0x28 x, +0x2C y? verified: 0x28 pair, 0x30 z]
namespace CameraHook {
    bool Init();
    void Shutdown();
}
