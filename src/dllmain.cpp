#include <Windows.h>
#include <iostream>
#include <thread>
#include "hooks/hooks.h"
#include "memory/mem.h"

void MainThread(HMODULE hModule) {
    // Alloc debug console
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);

    std::cout << "========================================\n";
    std::cout << "  MCBE Client - Loaded\n";
    std::cout << "  Target: Minecraft Bedrock 1.21.45\n";
    std::cout << "========================================\n";

    // Get base address of Minecraft.Windows.exe
    uintptr_t baseAddr = Mem::GetModuleBase("Minecraft.Windows.exe");
    if (baseAddr == 0) {
        std::cout << "[!] Failed to find Minecraft.Windows.exe base address\n";
        goto cleanup;
    }
    std::cout << "[+] Base address: 0x" << std::hex << baseAddr << std::dec << "\n";

    // Init hooks
    if (!Hooks::Init(baseAddr)) {
        std::cout << "[!] Hook init failed\n";
        goto cleanup;
    }
    std::cout << "[+] Hooks initialized\n";
    std::cout << "[*] Press END to eject\n\n";

    // Main loop - wait for eject key
    while (true) {
        if (GetAsyncKeyState(VK_END) & 1) {
            break;
        }
        Sleep(50);
    }

cleanup:
    std::cout << "[*] Ejecting...\n";
    Hooks::Shutdown();
    Sleep(200);

    if (f) fclose(f);
    FreeConsole();
    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CloseHandle(CreateThread(nullptr, 0,
            (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr));
    }
    return TRUE;
}
