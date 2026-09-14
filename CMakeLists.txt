cmake_minimum_required(VERSION 3.20)
project(MCBEClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(MCBEClient SHARED
    src/dllmain.cpp
    src/hooks/hooks.cpp
    src/memory/mem.cpp
)

target_include_directories(MCBEClient PRIVATE src)
