#include "auto_sprint.h"
#include <Windows.h>
#include <imgui.h>

AutoSprint* g_auto_sprint = nullptr;

AutoSprint::AutoSprint()
    : Module("Auto Sprint", "Holds sprint (Ctrl) automatically while moving forward", VK_F5) {
    g_auto_sprint = this;
}

// Called every frame from the render hook.
// Approach: instead of poking ECS movement state (fragile), we synthesize the
// sprint key the game already listens for. Bedrock Windows default sprint = Ctrl.
// We hold Ctrl while W is down and the MC window is focused, release otherwise.
void AutoSprint::Tick() {
    static bool ctrlHeld = false;

    bool shouldSprint = false;

    if (g_auto_sprint && g_auto_sprint->enabled) {
        // Only act when the Minecraft window is the foreground window, so we
        // never inject Ctrl into other apps while alt-tabbed.
        HWND fg = GetForegroundWindow();
        DWORD fgPid = 0;
        GetWindowThreadProcessId(fg, &fgPid);
        if (fgPid == GetCurrentProcessId()) {
            // W held = moving forward -> sprint
            if (GetAsyncKeyState('W') & 0x8000) {
                shouldSprint = true;
            }
        }
    }

    if (shouldSprint && !ctrlHeld) {
        INPUT in = {};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = VK_CONTROL;
        SendInput(1, &in, sizeof(INPUT));
        ctrlHeld = true;
    } else if (!shouldSprint && ctrlHeld) {
        INPUT in = {};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = VK_CONTROL;
        in.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(INPUT));
        ctrlHeld = false;
    }
}

void AutoSprint::OnRenderUI() {
    ImGui::Checkbox("Auto Sprint", &enabled);
    ImGui::TextDisabled("Holds Ctrl while W is down");
}
