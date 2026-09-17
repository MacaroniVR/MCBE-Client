#include <Windows.h>
#include <iostream>
#include <thread>
#include "hooks/hooks.h"

static HMODULE g_hModule = nullptr;

void MainThread(HMODULE hModule) {
    // Debug console
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);

    std::cout << "========================================\n";
    std::cout << "  MCBE Client - Loaded\n";
    std::cout << "  Target: Minecraft Bedrock 1.21.45\n";
    std::cout << "========================================\n";

    if (!Hooks::Init()) {
        std::cout << "[!] Hook init failed\n";
        goto cleanup;
    }

    std::cout << "[+] All hooks initialized\n";
    std::cout << "[*] Press INSERT to toggle menu\n";
    std::cout << "[*] Press END to eject\n\n";

    while (true) {
        if (GetAsyncKeyState(VK_END) & 1) {
            break;
        }
        Sleep(50);
    }

cleanup:
    std::cout << "[*] Ejecting...\n";
    Hooks::Shutdown();
    Sleep(300);

    if (f) fclose(f);
    FreeConsole();
    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CloseHandle(CreateThread(nullptr, 0,
            (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr));
    }
    return TRUE;
}
