#include "hooks.h"
#include "d3d11_hook.h"
#include "../memory/mem.h"
#include <MinHook.h>
#include <iostream>

namespace Hooks {

    bool Init() {
        uintptr_t base = Mem::GetModuleBase("Minecraft.Windows.exe");
        if (!base) {
            std::cout << "[!] Minecraft.Windows.exe not found\n";
            return false;
        }
        std::cout << "[+] Base: 0x" << std::hex << base << std::dec << "\n";

        if (MH_Initialize() != MH_OK) {
            std::cout << "[!] MinHook init failed\n";
            return false;
        }
        std::cout << "[+] MinHook initialized\n";

        if (!D3D11Hook::Init()) {
            std::cout << "[!] D3D11 hook failed\n";
            return false;
        }
        std::cout << "[+] D3D11 hook active\n";

        return true;
    }

    void Shutdown() {
        D3D11Hook::Shutdown();
        MH_Uninitialize();
        std::cout << "[+] All hooks removed\n";
    }
}
