#include "d3d12_hook.h"
#include "../ui/menu.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <vector>
#include <iostream>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace D3D12Hook {

    // ── Function types ──
    using PresentFn = HRESULT(__stdcall*)(IDXGISwapChain3*, UINT, UINT);
    using ExecuteCommandListsFn = void(__stdcall*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
    using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    // ── Per-frame context ──
    struct FrameContext {
        ID3D12CommandAllocator* commandAllocator = nullptr;
        ID3D12Resource* renderTarget = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
    };

    // ── Originals ──
    static PresentFn oPresent = nullptr;
    static ExecuteCommandListsFn oExecuteCommandLists = nullptr;
    static ResizeBuffersFn oResizeBuffers = nullptr;

    // ── State ──
    static ID3D12Device* g_device = nullptr;
    static ID3D12CommandQueue* g_commandQueue = nullptr;
    static ID3D12GraphicsCommandList* g_commandList = nullptr;
    static ID3D12DescriptorHeap* g_rtvHeap = nullptr;   // back buffer RTVs
    static ID3D12DescriptorHeap* g_srvHeap = nullptr;   // ImGui font SRV
    static std::vector<FrameContext> g_frameContexts;
    static UINT g_bufferCount = 0;

    static HWND g_hwnd = nullptr;
    static WNDPROC g_origWndProc = nullptr;
    static bool g_initialized = false;
    static bool g_menuOpen = true;

    // ── WndProc ──
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

    static void CleanupRenderTargets() {
        for (auto& ctx : g_frameContexts) {
            if (ctx.renderTarget) { ctx.renderTarget->Release(); ctx.renderTarget = nullptr; }
        }
    }

    // ── ExecuteCommandLists hook — captures the real command queue ──
    static void __stdcall hkExecuteCommandLists(ID3D12CommandQueue* queue, UINT numLists, ID3D12CommandList* const* lists) {
        if (!g_commandQueue && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            g_commandQueue = queue;
            std::cout << "[+] Captured command queue: 0x" << std::hex << (uintptr_t)queue << std::dec << "\n";
        }
        oExecuteCommandLists(queue, numLists, lists);
    }

    // ── Setup ImGui + render targets on first Present ──
    static bool SetupRenderer(IDXGISwapChain3* swapchain) {
        if (FAILED(swapchain->GetDevice(__uuidof(ID3D12Device), (void**)&g_device))) {
            std::cout << "[!] GetDevice (D3D12) failed\n";
            return false;
        }

        DXGI_SWAP_CHAIN_DESC desc;
        swapchain->GetDesc(&desc);
        g_hwnd = desc.OutputWindow;
        if (!g_hwnd) g_hwnd = FindWindowA(nullptr, "Minecraft");
        if (!g_hwnd) {
            std::cout << "[!] No game window\n";
            return false;
        }

        g_bufferCount = desc.BufferCount;
        g_frameContexts.clear();
        g_frameContexts.resize(g_bufferCount);

        // SRV heap for ImGui fonts
        {
            D3D12_DESCRIPTOR_HEAP_DESC d = {};
            d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            d.NumDescriptors = 1;
            d.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (FAILED(g_device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&g_srvHeap)))) {
                std::cout << "[!] SRV heap creation failed\n";
                return false;
            }
        }

        // RTV heap for back buffers
        {
            D3D12_DESCRIPTOR_HEAP_DESC d = {};
            d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            d.NumDescriptors = g_bufferCount;
            d.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            if (FAILED(g_device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&g_rtvHeap)))) {
                std::cout << "[!] RTV heap creation failed\n";
                return false;
            }

            UINT rtvSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();

            for (UINT i = 0; i < g_bufferCount; i++) {
                g_frameContexts[i].rtvHandle = rtvHandle;
                swapchain->GetBuffer(i, IID_PPV_ARGS(&g_frameContexts[i].renderTarget));
                g_device->CreateRenderTargetView(g_frameContexts[i].renderTarget, nullptr, rtvHandle);
                rtvHandle.ptr += rtvSize;
            }
        }

        // Command allocators (one per frame) + a single command list
        for (UINT i = 0; i < g_bufferCount; i++) {
            if (FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&g_frameContexts[i].commandAllocator)))) {
                std::cout << "[!] Command allocator failed\n";
                return false;
            }
        }

        if (FAILED(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            g_frameContexts[0].commandAllocator, nullptr, IID_PPV_ARGS(&g_commandList)))) {
            std::cout << "[!] Command list failed\n";
            return false;
        }
        g_commandList->Close();

        // Hook the window
        g_origWndProc = (WNDPROC)SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

        // Init ImGui
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX12_Init(
            g_device, g_bufferCount,
            DXGI_FORMAT_R8G8B8A8_UNORM, g_srvHeap,
            g_srvHeap->GetCPUDescriptorHandleForHeapStart(),
            g_srvHeap->GetGPUDescriptorHandleForHeapStart()
        );

        Menu::ApplyStyle();
        return true;
    }

    // ── Present hook ──
    static HRESULT __stdcall hkPresent(IDXGISwapChain3* swapchain, UINT syncInterval, UINT flags) {
        if (!g_initialized) {
            if (!g_commandQueue) {
                // Queue not captured yet — wait for ExecuteCommandLists to fire
                return oPresent(swapchain, syncInterval, flags);
            }
            if (SetupRenderer(swapchain)) {
                g_initialized = true;
                std::cout << "[+] ImGui (D3D12) initialized\n";
            } else {
                return oPresent(swapchain, syncInterval, flags);
            }
        }

        // New frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (g_menuOpen) {
            Menu::Render();
        }

        ImGui::Render();

        // Get current back buffer
        UINT backBufferIdx = swapchain->GetCurrentBackBufferIndex();
        FrameContext& frame = g_frameContexts[backBufferIdx];

        frame.commandAllocator->Reset();

        // Transition back buffer: PRESENT -> RENDER_TARGET
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = frame.renderTarget;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        g_commandList->Reset(frame.commandAllocator, nullptr);
        g_commandList->ResourceBarrier(1, &barrier);
        g_commandList->OMSetRenderTargets(1, &frame.rtvHandle, FALSE, nullptr);
        g_commandList->SetDescriptorHeaps(1, &g_srvHeap);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_commandList);

        // Transition back: RENDER_TARGET -> PRESENT
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        g_commandList->ResourceBarrier(1, &barrier);
        g_commandList->Close();

        // Execute on the captured command queue
        ID3D12CommandList* lists[] = { g_commandList };
        g_commandQueue->ExecuteCommandLists(1, lists);

        return oPresent(swapchain, syncInterval, flags);
    }

    // ── ResizeBuffers hook ──
    static HRESULT __stdcall hkResizeBuffers(IDXGISwapChain3* swapchain, UINT bufferCount,
        UINT width, UINT height, DXGI_FORMAT format, UINT flags) {

        CleanupRenderTargets();
        HRESULT hr = oResizeBuffers(swapchain, bufferCount, width, height, format, flags);

        if (g_initialized) {
            // Recreate RTVs
            UINT rtvSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
            for (UINT i = 0; i < g_bufferCount; i++) {
                g_frameContexts[i].rtvHandle = rtvHandle;
                swapchain->GetBuffer(i, IID_PPV_ARGS(&g_frameContexts[i].renderTarget));
                g_device->CreateRenderTargetView(g_frameContexts[i].renderTarget, nullptr, rtvHandle);
                rtvHandle.ptr += rtvSize;
            }
        }
        return hr;
    }

    // ── Init: build dummy device to grab vtables ──
    bool Init() {
        // Need a dummy swapchain + command queue to read vtable slots
        WNDCLASSEXA wc = { sizeof(WNDCLASSEXA), CS_CLASSDC, DefWindowProcA, 0, 0,
                           GetModuleHandleA(nullptr), nullptr, nullptr, nullptr, nullptr,
                           "DX12", nullptr };
        RegisterClassExA(&wc);
        HWND tempHwnd = CreateWindowA("DX12", "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100,
                                       nullptr, nullptr, wc.hInstance, nullptr);

        // Create device
        ID3D12Device* dummyDevice = nullptr;
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dummyDevice)))) {
            std::cout << "[!] D3D12CreateDevice failed\n";
            DestroyWindow(tempHwnd);
            UnregisterClassA("DX12", wc.hInstance);
            return false;
        }

        // Create command queue
        D3D12_COMMAND_QUEUE_DESC qDesc = {};
        qDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ID3D12CommandQueue* dummyQueue = nullptr;
        dummyDevice->CreateCommandQueue(&qDesc, IID_PPV_ARGS(&dummyQueue));

        // Create DXGI factory + swapchain
        IDXGIFactory4* factory = nullptr;
        CreateDXGIFactory1(IID_PPV_ARGS(&factory));

        DXGI_SWAP_CHAIN_DESC1 scDesc = {};
        scDesc.BufferCount = 2;
        scDesc.Width = 100;
        scDesc.Height = 100;
        scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scDesc.SampleDesc.Count = 1;

        IDXGISwapChain1* dummySC1 = nullptr;
        factory->CreateSwapChainForHwnd(dummyQueue, tempHwnd, &scDesc, nullptr, nullptr, &dummySC1);

        IDXGISwapChain3* dummySC = nullptr;
        void* presentTarget = nullptr;
        void* resizeTarget = nullptr;
        if (dummySC1 && SUCCEEDED(dummySC1->QueryInterface(IID_PPV_ARGS(&dummySC)))) {
            void** scVtable = *reinterpret_cast<void***>(dummySC);
            presentTarget = scVtable[8];    // Present
            resizeTarget = scVtable[13];    // ResizeBuffers
        }

        void* executeTarget = nullptr;
        if (dummyQueue) {
            void** qVtable = *reinterpret_cast<void***>(dummyQueue);
            executeTarget = qVtable[10];    // ExecuteCommandLists
        }

        std::cout << "[*] Present             @ 0x" << std::hex << (uintptr_t)presentTarget << "\n";
        std::cout << "[*] ExecuteCommandLists @ 0x" << std::hex << (uintptr_t)executeTarget << "\n";
        std::cout << std::dec;

        // Cleanup dummy objects
        if (dummySC) dummySC->Release();
        if (dummySC1) dummySC1->Release();
        if (factory) factory->Release();
        if (dummyQueue) dummyQueue->Release();
        if (dummyDevice) dummyDevice->Release();
        DestroyWindow(tempHwnd);
        UnregisterClassA("DX12", wc.hInstance);

        if (!presentTarget || !executeTarget) {
            std::cout << "[!] Failed to resolve vtable targets\n";
            return false;
        }

        // Hook ExecuteCommandLists (to capture the queue)
        if (MH_CreateHook(executeTarget, hkExecuteCommandLists, (void**)&oExecuteCommandLists) == MH_OK) {
            MH_EnableHook(executeTarget);
            std::cout << "[+] Hooked ExecuteCommandLists\n";
        } else {
            std::cout << "[!] Failed to hook ExecuteCommandLists\n";
            return false;
        }

        // Hook Present
        if (MH_CreateHook(presentTarget, hkPresent, (void**)&oPresent) == MH_OK) {
            MH_EnableHook(presentTarget);
            std::cout << "[+] Hooked Present\n";
        } else {
            std::cout << "[!] Failed to hook Present\n";
            return false;
        }

        // Hook ResizeBuffers
        if (resizeTarget && MH_CreateHook(resizeTarget, hkResizeBuffers, (void**)&oResizeBuffers) == MH_OK) {
            MH_EnableHook(resizeTarget);
        }

        return true;
    }

    void Shutdown() {
        MH_DisableHook(MH_ALL_HOOKS);

        if (g_initialized) {
            ImGui_ImplDX12_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }

        if (g_origWndProc && g_hwnd) {
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_origWndProc);
        }

        CleanupRenderTargets();
        for (auto& ctx : g_frameContexts) {
            if (ctx.commandAllocator) { ctx.commandAllocator->Release(); ctx.commandAllocator = nullptr; }
        }
        if (g_commandList) { g_commandList->Release(); g_commandList = nullptr; }
        if (g_rtvHeap) { g_rtvHeap->Release(); g_rtvHeap = nullptr; }
        if (g_srvHeap) { g_srvHeap->Release(); g_srvHeap = nullptr; }
    }

    bool IsMenuOpen() { return g_menuOpen; }
}
