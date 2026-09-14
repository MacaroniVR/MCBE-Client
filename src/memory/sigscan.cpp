#include "sigscan.h"
#include "mem.h"
#include <Windows.h>
#include <Psapi.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>

namespace SigScan {

    static bool ParsePattern(const char* ida, std::vector<uint8_t>& bytes, std::vector<uint8_t>& mask) {
        std::istringstream iss(ida);
        std::string token;
        while (iss >> token) {
            if (token == "?" || token == "??") {
                bytes.push_back(0);
                mask.push_back(0);
            } else {
                bytes.push_back((uint8_t)strtol(token.c_str(), nullptr, 16));
                mask.push_back(0xFF);
            }
        }
        return !bytes.empty();
    }

    uintptr_t Find(const char* idaPattern) {
        HMODULE hMod = GetModuleHandleA("Minecraft.Windows.exe");
        if (!hMod) return 0;

        MODULEINFO modInfo;
        if (!GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo)))
            return 0;

        std::vector<uint8_t> bytes, mask;
        if (!ParsePattern(idaPattern, bytes, mask)) return 0;

        uintptr_t base = reinterpret_cast<uintptr_t>(hMod);
        size_t size = modInfo.SizeOfImage;
        size_t patLen = bytes.size();

        for (size_t i = 0; i + patLen < size; i++) {
            bool found = true;
            for (size_t j = 0; j < patLen; j++) {
                if (mask[j] && *reinterpret_cast<uint8_t*>(base + i + j) != bytes[j]) {
                    found = false;
                    break;
                }
            }
            if (found) return base + i;
        }
        return 0;
    }

    struct SigEntry {
        const char* name;
        const char* pattern;
    };

    void RunTest() {
        // Signatures from Latite Client (LatiteClient/Latite, Sept 2026, actively maintained)
        static const SigEntry sigs[] = {
            { "Platform_GameCore",
              "4C 89 3D ? ? ? ? 4D 85 FF" },

            { "LevelRenderer::renderLevel",
              "E8 ? ? ? ? 45 31 E4 48 83 BE" },

            { "MultiPlayerLevel::_subTick",
              "55 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? 44 0F 29 6D" },

            { "Actor::setNameTag",
              "56 57 48 83 EC ? 48 89 CE 48 8B 89 ? ? ? ? 48 85 C9 0F 84 ? ? ? ? 48 89 D7" },

            { "LocalPlayer::applyTurnDelta",
              "55 41 56 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? 44 0F 29 5D ? 44 0F 29 55 ? 44 0F 29 4D ? 44 0F 29 45 ? 0F 29 7D ? 0F 29 75 ? 48 C7 45 ? ? ? ? ? 48 89 D7 48 89 CE 48 8B 89" },

            { "UpdatePlayerFromCameraSystemUtil::_updatePlayer",
              "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC ? ? ? ? 44 0F 29 94 24 ? ? ? ? 44 0F 29 8C 24 ? ? ? ? 44 0F 29 84 24 ? ? ? ? 0F 29 BC 24 ? ? ? ? 0F 29 B4 24 ? ? ? ? 4C 89 C6" },
        };

        uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA("Minecraft.Windows.exe"));

        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "  SIGNATURE SCAN TEST (Latite sigs)\n";
        std::cout << "  Base: 0x" << std::hex << base << std::dec << "\n";
        std::cout << "========================================\n";

        int hits = 0;
        int total = sizeof(sigs) / sizeof(sigs[0]);

        for (const auto& s : sigs) {
            uintptr_t addr = Find(s.pattern);
            if (addr) {
                hits++;
                std::cout << "[+] HIT  " << s.name << "\n";
                std::cout << "         @ 0x" << std::hex << addr
                          << "  (base+0x" << (addr - base) << ")" << std::dec << "\n";
            } else {
                std::cout << "[-] MISS " << s.name << "\n";
            }
        }

        std::cout << "----------------------------------------\n";
        std::cout << "  Result: " << hits << " / " << total << " signatures found\n";
        if (hits == total) {
            std::cout << "  All matched. Latite sigs work on your build.\n";
        } else if (hits == 0) {
            std::cout << "  Nothing matched.\n";
        } else {
            std::cout << "  Partial match.\n";
        }
        std::cout << "========================================\n\n";
    }
}
