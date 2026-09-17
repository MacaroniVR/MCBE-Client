#pragma once
#include <string>

class Module {
public:
    std::string name;
    std::string description;
    bool enabled = false;
    int keybind = 0;         // VK_ code or ASCII char, 0 = none
    int defaultKeybind = 0;  // what the reset (⟳) button restores to
    bool holdToActivate = false; // true = works while key held (no on/off toggle)

    Module(const std::string& name, const std::string& desc, int key = 0)
        : name(name), description(desc), keybind(key), defaultKeybind(key) {}

    virtual ~Module() = default;

    void Toggle() {
        enabled = !enabled;
        if (enabled) OnEnable();
        else OnDisable();
    }

    virtual void OnEnable() {}
    virtual void OnDisable() {}
    virtual void OnTick() {}
    virtual void OnRenderUI() {} // extra per-module ImGui widgets
};
