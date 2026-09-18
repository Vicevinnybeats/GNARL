cmake_minimum_required(VERSION 3.22)

project(GNARL
    VERSION 0.1.0
    DESCRIPTION "GNARL - riddim/dubstep synthesizer"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Universal binaries on macOS so one build serves Intel + Apple Silicon.
if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "" FORCE)
endif()
if (APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "" FORCE)
endif()

if (MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
    # Static runtime keeps the VST3 loadable on machines without the VC redist.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-Wall -Wextra -Wshadow -Wno-unused-parameter)
endif()

option(GNARL_BUILD_TESTS "Build the Catch2 DSP test suite" ON)
option(GNARL_BUILD_UI "Run the Vite build for the web UI as part of the CMake build" ON)
option(GNARL_BUILD_DEMO_RENDERER "Build tools/render_demo, which renders demo clips to .wav" OFF)

include(cmake/Dependencies.cmake)

add_subdirectory(plugin)

if (GNARL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
