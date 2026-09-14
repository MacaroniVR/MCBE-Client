#include "sigscan.h"
#include "mem.h"
#include <Windows.h>
#include <Psapi.h>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>

namespace SigScan {

    // Parse "48 8B ? ? C3" into bytes + mask (0xFF = must match, 0x00 = wildcard)
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
        // Signatures pulled from HorionContinued dump (modern ECS Bedrock)
        static const SigEntry sigs[] = {
            { "UpdateRenderPosSystem::_doUpdateRenderPosSystem",
              "8B 02 41 89 00 8B 42 04 41 89 40 ? 8B 42 08 41 89 40 ? C3" },

            { "ComponentDiff<RenderPositionComponent>::getDiff",
              "48 89 5C 24 ? 48 89 74 24 ? 57 48 81 EC 80 00 00 00 48 8B D9 33 F6 89 74 24 ? F3 0F 10 4A ? F3 41 0F 5C 48 ? F3 0F 10 52 ?" },

            { "Actor::updateEntityInside",
              "48 83 EC 28 48 8B 91 ? ? ? ? 48 85 D2 74 15" },

            { "AABB::translateCenterTo",
              "48 89 5C 24 ? 57 48 83 EC 30 48 8B DA 48 8B F9 48 8D 54 24 ?" },

            { "LoopbackPacketSender::sendToServer",
              "48 89 5C 24 ? 57 48 81 EC C0 00 00 00 0F B6 41 ?" },

            { "Actor::getPosPrev",
              "48 83 EC 28 48 8B 81 ? ? ? ? 48 85 C0 74 09 48 83 C0 0C" },
        };

        uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA("Minecraft.Windows.exe"));

        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "  SIGNATURE SCAN TEST\n";
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
            std::cout << "  All matched. Dump is compatible with your build.\n";
        } else if (hits == 0) {
            std::cout << "  Nothing matched. Wrong game version — need a fresh dump.\n";
        } else {
            std::cout << "  Partial match. Dump is close but not exact.\n";
        }
        std::cout << "========================================\n\n";
    }
}
