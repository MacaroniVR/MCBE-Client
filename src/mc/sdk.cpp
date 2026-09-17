#include "sdk.h"
#include "../memory/sigscan.h"
#include <Windows.h>

namespace SDK {

    // Platform_GameCore sig: "4C 89 3D ? ? ? ? 4D 85 FF"  (deref(3) -> global ptr)
    // Chain (from Latite):
    //   winMain      = *(void**)(Platform_GameCore global)
    //   platform     = *(Platform_GameCore**)(winMain + 0x8)
    //   minecraftGame= *(MinecraftGame**)(platform + 0x18)
    //   clientInst   = primaryClientInstance from map at (mcgame + 0x938), key 0
    //
    // The std::map walk is fragile to reimplement raw, so we resolve
    // ClientInstance a simpler way: MinecraftGame::getPrimaryClientInstance
    // stores the primary at map key 0. For a first pass we read the map node.

    static uintptr_t ResolveRipTarget(uintptr_t addr, int offsetPos) {
        // addr points at instruction; RIP-relative disp32 at offsetPos,
        // instruction length assumed offsetPos+4.
        int32_t disp = *reinterpret_cast<int32_t*>(addr + offsetPos);
        return addr + offsetPos + 4 + disp;
    }

    LocalPlayer* GetLocalPlayer() {
        uintptr_t sig = SigScan::Find("4C 89 3D ? ? ? ? 4D 85 FF");
        if (!sig) return nullptr;

        // "4C 89 3D <disp32>"  => mov [rip+disp32], r15
        // global ptr address = sig + 7 + disp32
        uintptr_t globalPtrAddr = ResolveRipTarget(sig, 3);
        if (!globalPtrAddr) return nullptr;

        void* winMain = *reinterpret_cast<void**>(globalPtrAddr);
        if (!winMain) return nullptr;

        void* platform = member_at<void*>(winMain, 0x8);
        if (!platform) return nullptr;

        void* mcgame = member_at<void*>(platform, 0x18);
        if (!mcgame) return nullptr;

        // primaryClientInstance: std::map<uint8_t, shared_ptr<ClientInstance>> at 0x938
        // For key 0, read the map root and find the first node.
        // std::map node layout (MSVC): _Myhead at map+0x0 ; node has
        //   [0] left [8] parent [0x10] right [0x18] color/isnil ...
        //   value (pair) at node + 0x20 -> pair<const uint8_t, shared_ptr>
        //   shared_ptr ptr at value + 0x8 (after the key + padding)
        uintptr_t mapAddr = reinterpret_cast<uintptr_t>(mcgame) + 0x938;
        void* head = *reinterpret_cast<void**>(mapAddr);          // _Myhead
        if (!head) return nullptr;
        void* firstNode = *reinterpret_cast<void**>(head);        // _Myhead->left = begin
        if (!firstNode || firstNode == head) return nullptr;

        // value pair at node + 0x20; shared_ptr control at +0x8 within pair
        uintptr_t pairAddr = reinterpret_cast<uintptr_t>(firstNode) + 0x20;
        // pair<const uint8_t, shared_ptr<ClientInstance>>:
        //   key at +0x0 (1 byte, padded), shared_ptr at +0x8
        void* ciPtr = *reinterpret_cast<void**>(pairAddr + 0x8);  // shared_ptr._Ptr
        if (!ciPtr) return nullptr;

        ClientInstance* ci = reinterpret_cast<ClientInstance*>(ciPtr);
        return ci->getLocalPlayer();
    }
}
