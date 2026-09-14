#include "d3d11_hook.h"
#include "../ui/menu.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <iostream>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace D3D11Hook {

    using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
    using Present1Fn = HRESULT(__stdcall*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
    using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    static PresentFn oPresent = nullptr;
    static Present1Fn oPresent1 = nullptr;
    static ResizeBuffersFn oResizeBuffers = nullptr;
    static ID3D11Device* g_device = nullptr;
    static ID3D11DeviceContext* g_context = nullptr;
    static ID3D11RenderTargetView* g_rtv = nullptr;
    static HWND g_hwnd = nullptr;
    static WNDPROC g_origWndProc = nullptr;
    static bool g_initialized = false;
    static bool g_menuOpen = true;

    static void CleanupRTV() {
        if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
    }

    static void CreateRTV(IDXGISwapChain* swapchain) {
        ID3D11Texture2D* backBuffer = nullptr;
        swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
        if (backBuffer) {
            g_device->CreateRenderTargetView(backBuffer, nullptr, &g_rtv);
            backBuffer->Release();
        }
    }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
            g_menuOpen = !g_menuOpen;
            return 0;
        }
        if (g_menuOpen && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
            return 0;
        }
        return CallWindowProcA(g_origWndProc, hWnd, msg, wParam, lParam);
    }

    static void InitImGui(IDXGISwapChain* swapchain) {
        if (g_initialized) return;

        std::cout << "[*] Present called — initializing ImGui...\n";

        if (FAILED(swapchain->GetDevice(__uuidof(ID3D11Device), (void**)&g_device))) {
            std::cout << "[!] GetDevice failed — game may use D3D12\n";
            return;
        }
        g_device->GetImmediateContext(&g_context);

        DXGI_SWAP_CHAIN_DESC desc;
        swapchain->GetDesc(&desc);
        g_hwnd = desc.OutputWindow;

        if (!g_hwnd) {
            g_hwnd = FindWindowA(nullptr, "Minecraft");
        }
        if (!g_hwnd) {
            std::cout << "[!] Could not find game window\n";
            return;
        }

        g_origWndProc = (WNDPROC)SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
        CreateRTV(swapchain);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX11_Init(g_device, g_context);
        Menu::ApplyStyle();

        g_initialized = true;
        std::cout << "[+] ImGui initialized\n";
    }

    static void RenderFrame() {
        if (!g_initialized) return;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (g_menuOpen) {
            Menu::Render();
        }

        ImGui::EndFrame();
        ImGui::Render();
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    static HRESULT __stdcall hkPresent(IDXGISwapChain* swapchain, UINT syncInterval, UINT flags) {
        InitImGui(swapchain);
        RenderFrame();
        return oPresent(swapchain, syncInterval, flags);
    }

    static HRESULT __stdcall hkPresent1(IDXGISwapChain1* swapchain, UINT syncInterval, UINT flags, const DXGI_PRESENT_PARAMETERS* params) {
        InitImGui(swapchain);
        RenderFrame();
        return oPresent1(swapchain, syncInterval, flags, params);
    }

    static HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* swapchain, UINT bufferCount,
        UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
        CleanupRTV();
        HRESULT hr = oResizeBuffers(swapchain, bufferCount, width, height, format, flags);
        if (g_initialized) CreateRTV(swapchain);
        return hr;
    }

    bool Init() {
        WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_CLASSDC, DefWindowProcA, 0, 0,
                           GetModuleHandleA(nullptr), nullptr, nullptr, nullptr, nullptr,
                           "DX", nullptr };
        RegisterClassExA(&wc);
        HWND tempHwnd = CreateWindowA("DX", "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100,
                                       nullptr, nullptr, wc.hInstance, nullptr);

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.Width = 2;
        sd.BufferDesc.Height = 2;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = tempHwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        ID3D11Device* tmpDevice = nullptr;
        IDXGISwapChain* tmpSwapchain = nullptr;
        ID3D11DeviceContext* tmpContext = nullptr;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            &featureLevel, 1, D3D11_SDK_VERSION,
            &sd, &tmpSwapchain, &tmpDevice, nullptr, &tmpContext
        );

        if (FAILED(hr)) {
            sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            sd.BufferCount = 1;
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                &featureLevel, 1, D3D11_SDK_VERSION,
                &sd, &tmpSwapchain, &tmpDevice, nullptr, &tmpContext
            );
        }

        if (FAILED(hr)) {
            std::cout << "[!] Dummy device failed: 0x" << std::hex << hr << std::dec << "\n";
            DestroyWindow(tempHwnd);
            UnregisterClassA("DX", wc.hInstance);
            return false;
        }

        void** scVtable = *reinterpret_cast<void***>(tmpSwapchain);
        void* presentTarget = scVtable[8];
        void* resizeTarget = scVtable[13];

        IDXGISwapChain1* tmpSC1 = nullptr;
        void* present1Target = nullptr;
        if (SUCCEEDED(tmpSwapchain->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&tmpSC1)) && tmpSC1) {
            void** sc1Vtable = *reinterpret_cast<void***>(tmpSC1);
            present1Target = sc1Vtable[22];
            tmpSC1->Release();
        }

        std::cout << "[*] Present  @ 0x" << std::hex << (uintptr_t)presentTarget << "\n";
        if (present1Target)
            std::cout << "[*] Present1 @ 0x" << std::hex << (uintptr_t)present1Target << "\n";
        std::cout << std::dec;

        tmpSwapchain->Release();
        tmpDevice->Release();
        tmpContext->Release();
        DestroyWindow(tempHwnd);
        UnregisterClassA("DX", wc.hInstance);

        // Hook Present
        if (MH_CreateHook(presentTarget, hkPresent, (void**)&oPresent) == MH_OK) {
            MH_EnableHook(presentTarget);
            std::cout << "[+] Hooked Present\n";
        } else {
            std::cout << "[!] Failed to hook Present\n";
        }

        // Hook Present1
        if (present1Target) {
            if (MH_CreateHook(present1Target, hkPresent1, (void**)&oPresent1) == MH_OK) {
                MH_EnableHook(present1Target);
                std::cout << "[+] Hooked Present1\n";
            } else {
                std::cout << "[!] Failed to hook Present1\n";
            }
        }

        // Hook ResizeBuffers
        if (MH_CreateHook(resizeTarget, hkResizeBuffers, (void**)&oResizeBuffers) == MH_OK) {
            MH_EnableHook(resizeTarget);
        }

        return true;
    }

    void Shutdown() {
        MH_DisableHook(MH_ALL_HOOKS);
        if (g_initialized) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
        CleanupRTV();
        if (g_origWndProc && g_hwnd)
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
        if (g_context) { g_context->Release(); g_context = nullptr; }
        if (g_device) { g_device->Release(); g_device = nullptr; }
    }

    bool IsMenuOpen() { return g_menuOpen; }
}
