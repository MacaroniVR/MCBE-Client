#include <Windows.h>
#include "menu.h"
#include "../modules/module.h"
#include "../modules/freecam.h"
#include "../modules/auto_sprint.h"
#include "../modules/durability_display.h"
#include "../modules/zoom.h"
#include "../modules/fps_counter.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>
#include <memory>
#include <string>
#include <cstdio>

namespace Menu {

    // ── Module registry ──
    static std::vector<std::unique_ptr<Module>> g_modules;
    static bool g_modulesInit = false;
    static int g_selectedTab = 0;

    // Which module is currently waiting to capture a key (nullptr = none)
    static Module* g_listeningModule = nullptr;

    // Human-readable name for a VK / ASCII keybind code.
    static std::string KeyName(int vk) {
        if (vk == 0) return "None";
        if (vk >= 'A' && vk <= 'Z') return std::string(1, (char)vk);
        if (vk >= '0' && vk <= '9') return std::string(1, (char)vk);
        if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);
        switch (vk) {
            case VK_SPACE:   return "Space";
            case VK_LSHIFT: case VK_SHIFT:   return "Shift";
            case VK_LCONTROL: case VK_CONTROL: return "Ctrl";
            case VK_LMENU: case VK_MENU:     return "Alt";
            case VK_TAB:     return "Tab";
            case VK_CAPITAL: return "Caps";
            case VK_RETURN:  return "Enter";
            case VK_BACK:    return "Backspace";
            case VK_LEFT:    return "Left";
            case VK_RIGHT:   return "Right";
            case VK_UP:      return "Up";
            case VK_DOWN:    return "Down";
            case VK_XBUTTON1: return "Mouse4";
            case VK_XBUTTON2: return "Mouse5";
            case VK_MBUTTON:  return "Mouse3";
            case VK_OEM_3:   return "`";
            default: {
                char buf[16];
                sprintf_s(buf, "0x%02X", vk);
                return std::string(buf);
            }
        }
    }

    // Poll for any key/mouse press to capture as a new bind. Returns 0 if nothing.
    static int PollAnyKey() {
        // Mouse extra buttons + middle (skip left/right — used for UI/gameplay)
        static const int mouse[] = { VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
        for (int m : mouse)
            if (GetAsyncKeyState(m) & 1) return m;
        // Keyboard: scan common range
        for (int vk = 0x08; vk <= 0xFE; vk++) {
            // Skip mouse L/R (0x01/0x02) so a click to bind doesn't self-capture
            if (vk == VK_LBUTTON || vk == VK_RBUTTON) continue;
            if (GetAsyncKeyState(vk) & 1) return vk;
        }
        return 0;
    }


    static void InitModules() {
        if (g_modulesInit) return;
        g_modules.push_back(std::make_unique<Freecam>());
        g_modules.push_back(std::make_unique<AutoSprint>());
        g_modules.push_back(std::make_unique<DurabilityDisplay>());
        g_modules.push_back(std::make_unique<Zoom>());
        g_modules.push_back(std::make_unique<FpsCounter>());
        g_modulesInit = true;
    }

    // ── Custom toggle switch widget ──
    static bool ToggleSwitch(const char* label, bool* v) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);

        const float height = ImGui::GetFrameHeight() * 0.75f;
        const float width = height * 1.8f;
        const float radius = height * 0.5f;

        ImVec2 pos = window->DC.CursorPos;
        ImVec2 textSize = ImGui::CalcTextSize(label, nullptr, true);

        ImRect totalBB(pos, ImVec2(pos.x + width + style.ItemInnerSpacing.x + textSize.x, pos.y + height));
        ImGui::ItemSize(totalBB, style.FramePadding.y);
        if (!ImGui::ItemAdd(totalBB, id)) return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(totalBB, id, &hovered, &held);
        if (pressed) *v = !*v;

        // Animate
        float t = *v ? 1.0f : 0.0f;

        // Colors
        ImU32 bgColor;
        if (*v) {
            bgColor = IM_COL32(100, 180, 255, 255); // Blue when on
        } else {
            bgColor = IM_COL32(60, 63, 70, 255);    // Dark when off
        }
        if (hovered) {
            bgColor = *v ? IM_COL32(120, 195, 255, 255) : IM_COL32(75, 78, 85, 255);
        }

        ImDrawList* drawList = window->DrawList;
        ImVec2 bgMin = pos;
        ImVec2 bgMax = ImVec2(pos.x + width, pos.y + height);

        drawList->AddRectFilled(bgMin, bgMax, bgColor, radius);

        // Knob
        float knobX = *v ? (pos.x + width - radius) : (pos.x + radius);
        ImVec2 knobCenter(knobX, pos.y + radius);
        drawList->AddCircleFilled(knobCenter, radius - 2.0f, IM_COL32(255, 255, 255, 255));

        // Label — honor ImGui's "##" convention: text after (or starting with)
        // "##" is an ID only and must not be drawn. Find the visible portion.
        const char* labelEnd = label;
        while (*labelEnd && !(labelEnd[0] == '#' && labelEnd[1] == '#')) labelEnd++;
        if (labelEnd != label) {
            ImVec2 labelPos(pos.x + width + style.ItemInnerSpacing.x, pos.y + (height - textSize.y) * 0.5f);
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), labelPos,
                              ImGui::GetColorU32(ImGuiCol_Text), label, labelEnd);
        }

        return pressed;
    }

    // ── Styling ──
    void ApplyStyle() {
        ImGuiStyle& s = ImGui::GetStyle();
        ImVec4* c = s.Colors;

        // Rounding
        s.WindowRounding = 10.0f;
        s.ChildRounding = 8.0f;
        s.FrameRounding = 6.0f;
        s.GrabRounding = 6.0f;
        s.PopupRounding = 8.0f;
        s.ScrollbarRounding = 6.0f;
        s.TabRounding = 6.0f;

        // Spacing
        s.WindowPadding = ImVec2(16, 16);
        s.FramePadding = ImVec2(10, 6);
        s.ItemSpacing = ImVec2(10, 8);
        s.ItemInnerSpacing = ImVec2(8, 4);
        s.ScrollbarSize = 10.0f;

        s.WindowBorderSize = 0.0f;
        s.ChildBorderSize = 0.0f;
        s.FrameBorderSize = 0.0f;

        // Dark base
        c[ImGuiCol_WindowBg]           = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
        c[ImGuiCol_ChildBg]            = ImVec4(0.11f, 0.11f, 0.14f, 1.00f);
        c[ImGuiCol_PopupBg]            = ImVec4(0.10f, 0.10f, 0.13f, 0.95f);
        c[ImGuiCol_Border]             = ImVec4(0.20f, 0.20f, 0.25f, 0.50f);

        // Header (tabs, collapsing headers)
        c[ImGuiCol_Header]             = ImVec4(0.39f, 0.71f, 1.00f, 0.20f);
        c[ImGuiCol_HeaderHovered]      = ImVec4(0.39f, 0.71f, 1.00f, 0.35f);
        c[ImGuiCol_HeaderActive]       = ImVec4(0.39f, 0.71f, 1.00f, 0.45f);

        // Buttons
        c[ImGuiCol_Button]             = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
        c[ImGuiCol_ButtonHovered]      = ImVec4(0.39f, 0.71f, 1.00f, 0.40f);
        c[ImGuiCol_ButtonActive]       = ImVec4(0.39f, 0.71f, 1.00f, 0.60f);

        // Frame (inputs, sliders)
        c[ImGuiCol_FrameBg]            = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
        c[ImGuiCol_FrameBgHovered]     = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
        c[ImGuiCol_FrameBgActive]      = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);

        // Slider grab
        c[ImGuiCol_SliderGrab]         = ImVec4(0.39f, 0.71f, 1.00f, 0.80f);
        c[ImGuiCol_SliderGrabActive]   = ImVec4(0.50f, 0.78f, 1.00f, 1.00f);

        // Check mark
        c[ImGuiCol_CheckMark]          = ImVec4(0.39f, 0.71f, 1.00f, 1.00f);

        // Tab
        c[ImGuiCol_Tab]                = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
        c[ImGuiCol_TabHovered]         = ImVec4(0.39f, 0.71f, 1.00f, 0.40f);
        c[ImGuiCol_TabSelected]        = ImVec4(0.39f, 0.71f, 1.00f, 0.25f);

        // Title bar
        c[ImGuiCol_TitleBg]            = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);
        c[ImGuiCol_TitleBgActive]      = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);

        // Scrollbar
        c[ImGuiCol_ScrollbarBg]        = ImVec4(0.08f, 0.08f, 0.10f, 0.50f);
        c[ImGuiCol_ScrollbarGrab]      = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.45f, 1.00f);
        c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);

        // Separator
        c[ImGuiCol_Separator]          = ImVec4(0.20f, 0.20f, 0.25f, 0.50f);

        // Text
        c[ImGuiCol_Text]               = ImVec4(0.90f, 0.90f, 0.93f, 1.00f);
        c[ImGuiCol_TextDisabled]       = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);
    }

    // ── Poll ── runs every frame, menu open or not: module ticks + keybinds
    void Poll() {
        InitModules();

        // Tick all enabled modules
        for (auto& mod : g_modules) {
            if (mod->enabled) mod->OnTick();
        }

        // If a module is waiting for a new bind, capture the next key pressed.
        // Otherwise run normal keybind toggles.
        if (g_listeningModule) {
            int key = PollAnyKey();
            if (key == VK_ESCAPE) {
                g_listeningModule = nullptr;   // Esc cancels binding
            } else if (key != 0) {
                g_listeningModule->keybind = key;
                g_listeningModule = nullptr;
            }
        } else {
            // Handle keybinds (hold-to-activate modules manage their own key in Tick)
            for (auto& mod : g_modules) {
                if (mod->holdToActivate) continue;
                if (mod->keybind && (GetAsyncKeyState(mod->keybind) & 1)) {
                    mod->Toggle();
                }
            }
        }
    }

    // ── Render ── draws the menu window (only called when menu is open)
    void Render() {
        InitModules();

        ImGui::SetNextWindowSize(ImVec2(500, 420), ImGuiCond_FirstUseEver);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
        ImGui::Begin("MCBE Client", nullptr, flags);

        // ── Header ──
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wPos = ImGui::GetWindowPos();
            ImVec2 wSize = ImGui::GetWindowSize();

            // Accent line under title bar
            float titleBarH = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y;
            dl->AddRectFilled(
                ImVec2(wPos.x, wPos.y + titleBarH - 2),
                ImVec2(wPos.x + wSize.x, wPos.y + titleBarH),
                IM_COL32(100, 180, 255, 180)
            );
        }

        ImGui::Spacing();

        // ── Tabs ──
        const char* tabs[] = { "Modules", "Settings" };
        int tabCount = 2;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 8));
        for (int i = 0; i < tabCount; i++) {
            if (i > 0) ImGui::SameLine();

            bool selected = (g_selectedTab == i);
            if (selected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.39f, 0.71f, 1.00f, 0.30f));
            }

            if (ImGui::Button(tabs[i], ImVec2(90, 0))) {
                g_selectedTab = i;
            }

            if (selected) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::PopStyleVar();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // ── Tab content ──
        if (g_selectedTab == 0) {
            // Modules tab
            for (auto& mod : g_modules) {
                ImGui::PushID(mod->name.c_str());

                ImGui::BeginChild(mod->name.c_str(), ImVec2(0, 0), ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);

                // Toggle + name row (hold-to-activate modules have no on/off toggle)
                if (!mod->holdToActivate) {
                    ToggleSwitch("##toggle", &mod->enabled);
                    ImGui::SameLine();
                }

                // Module name large
                ImGui::PushStyleColor(ImGuiCol_Text,
                    mod->enabled ? ImVec4(0.39f, 0.71f, 1.00f, 1.0f) : ImVec4(0.90f, 0.90f, 0.93f, 1.0f));
                ImGui::Text("%s", mod->name.c_str());
                ImGui::PopStyleColor();

                // Description
                ImGui::TextDisabled("%s", mod->description.c_str());

                // ── Keybind row: [ Key ] [None] [⟳] ──
                {
                    bool listening = (g_listeningModule == mod.get());
                    const char* verb = mod->holdToActivate ? "Hold:" : "Bind:";
                    ImGui::TextDisabled("%s", verb);
                    ImGui::SameLine();

                    // Main bind button — shows current key, or "Listening..." when active
                    std::string btnLabel = listening ? "Listening..." : KeyName(mod->keybind);
                    btnLabel += "##bind";
                    if (listening)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.39f, 0.71f, 1.00f, 0.45f));
                    if (ImGui::Button(btnLabel.c_str(), ImVec2(110, 0))) {
                        g_listeningModule = listening ? nullptr : mod.get();
                    }
                    if (listening) ImGui::PopStyleColor();

                    // "None" — clear the bind
                    ImGui::SameLine();
                    if (ImGui::Button("None##none")) {
                        mod->keybind = 0;
                        if (listening) g_listeningModule = nullptr;
                    }

                    // "⟳" reset-to-default — only if this module HAS a default bind
                    if (mod->defaultKeybind != 0) {
                        ImGui::SameLine();
                        // U+27F3 CLOCKWISE GAPPED CIRCLE ARROW, UTF-8 encoded
                        if (ImGui::Button("\xE2\x9F\xB3##reset")) {
                            mod->keybind = mod->defaultKeybind;
                            if (listening) g_listeningModule = nullptr;
                        }
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Reset to default (%s)", KeyName(mod->defaultKeybind).c_str());
                    }
                }

                // Per-module settings
                mod->OnRenderUI();

                ImGui::EndChild();
                ImGui::Spacing();

                ImGui::PopID();
            }
        }
        else if (g_selectedTab == 1) {
            // Settings tab
            ImGui::Text("Client Settings");
            ImGui::Spacing();
            ImGui::TextDisabled("More settings coming soon.");
        }

        ImGui::End();
    }
}
