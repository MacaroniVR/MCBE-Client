#pragma once
#include <cstdint>

// Compact SDK ported from Latite (LatiteClient/Latite) — offsets verified
// against this build via the 6/6 signature match.

namespace SDK {

    struct Vec2 { float x, y; };
    struct Vec3 { float x, y, z; };
    struct Vec4 { float x, y, z, w; };

    struct StateVectorComponent {
        Vec3 pos;       // 0x00
        Vec3 posOld;    // 0x0C
        Vec3 velocity;  // 0x18
    };

    struct ActorRotationComponent {
        Vec2 rotation;  // 0x00
    };

    template<typename TRet, typename... TArgs>
    inline TRet callVirtual(void* thisptr, size_t index, TArgs... args) {
        using TFunc = TRet(__fastcall*)(void*, TArgs...);
        TFunc* vtable = *reinterpret_cast<TFunc**>(thisptr);
        return vtable[index](thisptr, args...);
    }

    template<typename T>
    inline T& member_at(void* base, size_t off) {
        return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + off);
    }

    // ── Actor / Player ──
    class Actor {
    public:
        StateVectorComponent* getStateVector() {
            return member_at<StateVectorComponent*>(this, 0x218);
        }
        ActorRotationComponent* getRotComp() {
            return member_at<ActorRotationComponent*>(this, 0x228);
        }
        Vec3& getPos()      { return getStateVector()->pos; }
        Vec3& getPosOld()   { return getStateVector()->posOld; }
        Vec3& getVelocity() { return getStateVector()->velocity; }
        Vec2& getRot()      { return getRotComp()->rotation; }
    };

    class LocalPlayer : public Actor {};

    // ── ClientInstance ──
    class ClientInstance {
    public:
        LocalPlayer* getLocalPlayer() {
            return callVirtual<LocalPlayer*>(this, 0x1F);
        }
    };

    // Resolves ClientInstance -> LocalPlayer through the GameCore chain.
    LocalPlayer* GetLocalPlayer();

    // CameraComponent — arg1 of _updatePlayer
    class CameraComponent {
    public:
        char pad_0000[0x30];    // 0x0000
        Vec4 lookAngles;        // 0x0030 (quaternion)
        Vec3 cameraPos;         // 0x0040
        Vec2 fov;               // 0x004C
        float nearClip;         // 0x0054
        float farClip;          // 0x0058
    };
}

namespace SDK {
    // Render camera chain — the actual world-draw origin
    class LevelRendererPlayer {
    public:
        Vec3& getOrigin() { return member_at<Vec3>(this, 0x660); }
    };

    class LevelRenderer {
    public:
        LevelRendererPlayer* getLevelRendererPlayer() {
            return member_at<LevelRendererPlayer*>(this, 0x468);
        }
    };
}
