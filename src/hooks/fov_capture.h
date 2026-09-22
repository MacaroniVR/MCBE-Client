#pragma once
#include <cstdint>

// Captures the FOV settings struct pointer by inline-hooking the FOV write site.
// The write instruction is `movss [rsi+0x18], xmm3`; RSI is the struct base.
// Signature (24 bytes, version-stable, ends AT the write instruction):
//   0F 28 DA F3 0F C2 D9 01 0F 28 CB 0F 55 C8 0F 54 DA 0F 56 D9 F3 0F 11 5E 18
// FOV value = struct + 0x18. (min/max live nearby; see notes.)
namespace FovCapture {
    bool Init();
    void Shutdown();

    // The captured FOV struct base (RSI at the write). 0 until the game writes FOV
    // at least once (happens on load / when the FOV setting is touched).
    extern volatile uintptr_t g_fovStruct;
}
