#pragma once
#include <cstdint>

// Captures the ClientInstance pointer by hooking ClientInstance::_updateScreenSizeVariables
// (this = ClientInstance, called regularly). ClientInstance is the root for the whole
// object chain (MinecraftGame, LevelRender, GameRenderer, LocalPlayer...).
//
// 1.21.5X resolved offsets (from Flarial dll-oss, cumulative <=1.21.5):
//   ClientInstance + 0xE8  -> LevelRender
//   LevelRender    + 0x318 -> LevelRendererPlayer
//   LevelRendererPlayer + 0x6E4 -> cameraPos Vec3  (blocks + world origin)
//   ClientInstance + 0xD0  -> MinecraftGame
namespace ClientCapture {
    bool Init();
    void Shutdown();

    extern volatile uintptr_t g_clientInstance;

    // Convenience walkers (return 0 on any bad pointer)
    uintptr_t GetLevelRendererPlayer();  // the struct holding cameraPos @ 0x6E4
}
