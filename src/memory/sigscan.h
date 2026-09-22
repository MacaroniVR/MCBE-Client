#pragma once
#include <cstdint>

namespace SigScan {
    // Scans Minecraft.Windows.exe for a IDA-style pattern ("48 8B ? ? C3").
    // Returns absolute address, or 0 if not found.
    uintptr_t Find(const char* idaPattern);

    // Runs the freecam-relevant signature test and prints results to console.
    void RunTest();
}
