#include "fov_capture.h"
#include "../memory/mem.h"
#include "../memory/sigscan.h"
#include <Windows.h>
#include <iostream>

namespace FovCapture {

    volatile uintptr_t g_fovStruct = 0;

    // Signature ending exactly at the FOV write instruction (movss [rsi+0x18],xmm3).
    // The write is the last 5 bytes (F3 0F 11 5E 18) at sig offset +20.
    static const char* SIG =
        "0F 28 DA F3 0F C2 D9 01 0F 28 CB 0F 55 C8 0F 54 DA 0F 56 D9 F3 0F 11 5E 18";

    static uintptr_t g_writeAddr = 0;   // address of `movss [rsi+0x18], xmm3`
    static BYTE*     g_trampoline = nullptr;
    static BYTE      g_original[14];     // saved bytes we overwrite

    // Our capture stub runs with RSI = FOV struct. It stores RSI, then the
    // trampoline re-executes the original movss and jumps back.
    // We implement the capture by redirecting the write instruction to a code cave
    // that: pushes regs, saves RSI -> g_fovStruct, pops regs, does the movss, jmps back.
    //
    // Simpler + safe approach: place a JMP at the write site to a cave. The cave:
    //   mov [g_fovStruct], rsi
    //   movss [rsi+0x18], xmm3     ; original instruction (5 bytes)
    //   jmp back to writeAddr+5
    // movss is 5 bytes; our JMP rel32 is 5 bytes, so we steal exactly 5 (no partial).

    bool Init() {
        uintptr_t sig = SigScan::Find(SIG);
        if (!sig) {
            std::cout << "[FovCapture] signature not found\n";
            return false;
        }
        g_writeAddr = sig + 20;  // movss [rsi+0x18], xmm3 starts here

        // Verify the 5 bytes are the expected movss (F3 0F 11 5E 18)
        BYTE* w = reinterpret_cast<BYTE*>(g_writeAddr);
        if (!(w[0]==0xF3 && w[1]==0x0F && w[2]==0x11 && w[3]==0x5E && w[4]==0x18)) {
            std::cout << "[FovCapture] write-site bytes mismatch, aborting\n";
            return false;
        }

        // Allocate a code cave WITHIN ±2GB of the target so rel32 jmps reach it.
        // VirtualAlloc(nullptr,...) can land >2GB away, which makes the rel32
        // overflow and jump into garbage -> crash. Scan downward from the target
        // for a free page in range.
        g_trampoline = nullptr;
        {
            const uintptr_t target = g_writeAddr;
            SYSTEM_INFO si; GetSystemInfo(&si);
            uintptr_t gran = si.dwAllocationGranularity;
            // Search a window of ~1.5GB below and above the target
            for (uintptr_t probe = (target & ~(gran - 1)) - gran;
                 probe > target - 0x60000000ULL && probe > gran; probe -= gran) {
                void* p = VirtualAlloc((void*)probe, 64, MEM_COMMIT | MEM_RESERVE,
                                       PAGE_EXECUTE_READWRITE);
                if (p) { g_trampoline = (BYTE*)p; break; }
            }
            if (!g_trampoline) {
                for (uintptr_t probe = (target & ~(gran - 1)) + gran;
                     probe < target + 0x60000000ULL; probe += gran) {
                    void* p = VirtualAlloc((void*)probe, 64, MEM_COMMIT | MEM_RESERVE,
                                           PAGE_EXECUTE_READWRITE);
                    if (p) { g_trampoline = (BYTE*)p; break; }
                }
            }
        }
        if (!g_trampoline) {
            std::cout << "[FovCapture] cave alloc (in-range) failed\n";
            return false;
        }

        // Build the cave:
        //   48 89 34 25 <abs32 addr of g_fovStruct>   mov [g_fovStruct], rsi  (needs rip-rel; use movabs instead)
        // Simpler: movabs rax, &g_fovStruct ; mov [rax], rsi  -- but that clobbers rax.
        // Safest: push rax; movabs rax,&g_fovStruct; mov [rax],rsi; pop rax; <movss>; jmp back
        BYTE* p = g_trampoline;

        *p++ = 0x50;                                  // push rax
        *p++ = 0x48; *p++ = 0xB8;                     // movabs rax, imm64
        uintptr_t pStruct = (uintptr_t)&g_fovStruct;
        memcpy(p, &pStruct, 8); p += 8;
        *p++ = 0x48; *p++ = 0x89; *p++ = 0x30;        // mov [rax], rsi
        *p++ = 0x58;                                  // pop rax

        // original movss [rsi+0x18], xmm3  (5 bytes)
        memcpy(p, w, 5); p += 5;

        // jmp back to g_writeAddr + 5
        uintptr_t backTarget = g_writeAddr + 5;
        int64_t relBack64 = (int64_t)backTarget - (int64_t)((uintptr_t)p + 5);
        if (relBack64 < INT32_MIN || relBack64 > INT32_MAX) {
            std::cout << "[FovCapture] back-jmp out of range, aborting\n";
            VirtualFree(g_trampoline, 0, MEM_RELEASE); g_trampoline = nullptr;
            return false;
        }
        *p++ = 0xE9;
        int32_t rel = (int32_t)relBack64;
        memcpy(p, &rel, 4); p += 4;

        // Verify the jmp to the cave fits in rel32 before touching code bytes
        int64_t relToCave64 = (int64_t)(uintptr_t)g_trampoline - (int64_t)(g_writeAddr + 5);
        if (relToCave64 < INT32_MIN || relToCave64 > INT32_MAX) {
            std::cout << "[FovCapture] cave-jmp out of range, aborting\n";
            VirtualFree(g_trampoline, 0, MEM_RELEASE); g_trampoline = nullptr;
            return false;
        }

        // Now patch the write site: JMP rel32 to cave (5 bytes, exactly the movss size)
        DWORD oldProt;
        VirtualProtect(w, 5, PAGE_EXECUTE_READWRITE, &oldProt);
        memcpy(g_original, w, 5);   // save for shutdown
        w[0] = 0xE9;
        int32_t relToCave = (int32_t)relToCave64;
        memcpy(w + 1, &relToCave, 4);
        VirtualProtect(w, 5, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), w, 5);

        std::cout << "[FovCapture] hooked FOV write @ " << std::hex << g_writeAddr << "\n";
        return true;
    }

    void Shutdown() {
        if (g_writeAddr) {
            BYTE* w = reinterpret_cast<BYTE*>(g_writeAddr);
            DWORD oldProt;
            VirtualProtect(w, 5, PAGE_EXECUTE_READWRITE, &oldProt);
            memcpy(w, g_original, 5);
            VirtualProtect(w, 5, oldProt, &oldProt);
        }
        if (g_trampoline) {
            VirtualFree(g_trampoline, 0, MEM_RELEASE);
            g_trampoline = nullptr;
        }
    }
}
