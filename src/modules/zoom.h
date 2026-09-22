#pragma once
#include "module.h"

// Zoom — scales the game's FOV setting while a key is held.
// The FOV struct pointer is captured live by FovCapture (inline hook on the
// FOV write site). FOV value lives at struct + 0x18. We lerp it down on hold and
// restore on release. No static offsets, no DMA chain — version-stable via sig.
class Zoom : public Module {
public:
    float zoomFov  = 5.0f;    // target FOV while held
    float lerpTime = 0.05f;   // per-frame lerp factor (higher = snappier)

    bool  active     = false;
    float savedFov   = 76.0f; // captured real FOV when zoom begins

    Zoom();
    void OnDisable() override;
    void OnRenderUI() override;

    static void Tick();       // called every frame from the render hook

private:
    float* GetFovPtr();       // struct + 0x18, or nullptr if not captured yet
};

extern Zoom* g_zoom;
