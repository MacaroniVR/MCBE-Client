#pragma once
#include <cstdint>

namespace Hooks {
    bool Init(uintptr_t baseAddr);
    void Shutdown();
}
