cmake_minimum_required(VERSION 3.20)
project(MCBEClient LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ── ImGui ──
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.0
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# ── MinHook ──
FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master
)
FetchContent_GetProperties(minhook)
if(NOT minhook_POPULATED)
    FetchContent_Populate(minhook)
endif()

add_library(minhook STATIC
    ${minhook_SOURCE_DIR}/src/buffer.c
    ${minhook_SOURCE_DIR}/src/hook.c
    ${minhook_SOURCE_DIR}/src/trampoline.c
    ${minhook_SOURCE_DIR}/src/hde/hde64.c
)
target_include_directories(minhook PUBLIC ${minhook_SOURCE_DIR}/include)

# ── MCBE Client DLL ──
add_library(MCBEClient SHARED
    src/dllmain.cpp
    src/hooks/hooks.cpp
    src/hooks/d3d12_hook.cpp
    src/hooks/game_hooks.cpp
    src/hooks/fov_capture.cpp
    src/ui/menu.cpp
    src/modules/freecam.cpp
    src/modules/auto_sprint.cpp
    src/modules/durability_display.cpp
    src/modules/zoom.cpp
    src/mc/sdk.cpp
    src/memory/mem.cpp
    src/memory/sigscan.cpp
)

target_include_directories(MCBEClient PRIVATE src)
target_link_libraries(MCBEClient PRIVATE imgui minhook d3d12 dxgi d3dcompiler)
