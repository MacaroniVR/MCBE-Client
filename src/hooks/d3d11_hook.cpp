#include "d3d11_hook.h"
#include "../ui/menu.h"
#include <d3d11.h>
#include <dxgi.h>
#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <iostream>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace D3D11Hook {

    // Types
    using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    // State
    static PresentFn oPresent = nullptr;
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
        // Toggle menu on INSERT
        if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
            g_menuOpen = !g_menuOpen;
            return 0;
        }

        // Forward to ImGui when menu is open
        if (g_menuOpen && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
            return 0;
        }

        return CallWindowProcA(g_origWndProc, hWnd, msg, wParam, lParam);
    }

    static HRESULT __stdcall hkPresent(IDXGISwapChain* swapchain, UINT syncInterval, UINT flags) {
        if (!g_initialized) {
            // Get device and context from swapchain
            if (SUCCEEDED(swapchain->GetDevice(__uuidof(ID3D11Device), (void**)&g_device))) {
                g_device->GetImmediateContext(&g_context);

                // Get HWND from swapchain desc
                DXGI_SWAP_CHAIN_DESC desc;
                swapchain->GetDesc(&desc);
                g_hwnd = desc.OutputWindow;

                // Hook WndProc
                g_origWndProc = (WNDPROC)SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

                // Create render target
                CreateRTV(swapchain);

                // Init ImGui
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

                ImGui_ImplWin32_Init(g_hwnd);
                ImGui_ImplDX11_Init(g_device, g_context);

                // Apply custom style
                Menu::ApplyStyle();

                std::cout << "[+] ImGui initialized\n";
                g_initialized = true;
            }
        }

        if (g_initialized) {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // Render menu
            if (g_menuOpen) {
                Menu::Render();
            }

            ImGui::EndFrame();
            ImGui::Render();

            g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        return oPresent(swapchain, syncInterval, flags);
    }

    static HRESULT __stdcall hkResizeBuffers(IDXGISwapChain* swapchain, UINT bufferCount,
        UINT width, UINT height, DXGI_FORMAT format, UINT flags) {

        CleanupRTV();

        HRESULT hr = oResizeBuffers(swapchain, bufferCount, width, height, format, flags);

        if (g_initialized) {
            CreateRTV(swapchain);
        }

        return hr;
    }

    bool Init() {
        // Create dummy device + swapchain to get vtable
        WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_CLASSDC, DefWindowProcA, 0, 0,
                           GetModuleHandleA(nullptr), nullptr, nullptr, nullptr, nullptr,
                           "DX", nullptr };
        RegisterClassExA(&wc);
        HWND tempHwnd = CreateWindowA("DX", "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100,
                                       nullptr, nullptr, wc.hInstance, nullptr);

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.Width = 2;
        sd.BufferDesc.Height = 2;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = tempHwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        ID3D11Device* tmpDevice = nullptr;
        IDXGISwapChain* tmpSwapchain = nullptr;
        ID3D11DeviceContext* tmpContext = nullptr;

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION,
            &sd, &tmpSwapchain, &tmpDevice, nullptr, &tmpContext
        );

        if (FAILED(hr)) {
            DestroyWindow(tempHwnd);
            UnregisterClassA("DX", wc.hInstance);
            return false;
        }

        // Get vtable pointers
        void** swapchainVtable = *reinterpret_cast<void***>(tmpSwapchain);
        void* presentTarget = swapchainVtable[8];   // Present
        void* resizeTarget = swapchainVtable[13];    // ResizeBuffers

        // Cleanup dummy
        tmpSwapchain->Release();
        tmpDevice->Release();
        tmpContext->Release();
        DestroyWindow(tempHwnd);
        UnregisterClassA("DX", wc.hInstance);

        // Hook Present
        if (MH_CreateHook(presentTarget, hkPresent, (void**)&oPresent) != MH_OK) {
            return false;
        }
        if (MH_EnableHook(presentTarget) != MH_OK) {
            return false;
        }

        // Hook ResizeBuffers
        if (MH_CreateHook(resizeTarget, hkResizeBuffers, (void**)&oResizeBuffers) != MH_OK) {
            return false;
        }
        if (MH_EnableHook(resizeTarget) != MH_OK) {
            return false;
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

        if (g_origWndProc && g_hwnd) {
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
        }

        if (g_context) { g_context->Release(); g_context = nullptr; }
        if (g_device) { g_device->Release(); g_device = nullptr; }
    }

    bool IsMenuOpen() {
        return g_menuOpen;
    }
}
