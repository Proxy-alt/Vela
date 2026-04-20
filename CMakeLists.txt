cmake_minimum_required(VERSION 3.24)
project(Vela
  VERSION 0.0.1
  DESCRIPTION "Nintendo Switch emulator, WASM-first C core"
  LANGUAGES C)

#
# Options (see docs/DESIGN.md section 20)
#
set(CPU_BACKEND "noop" CACHE STRING "CPU backend: noop|interpreter|dynarmic|ballistic")
set_property(CACHE CPU_BACKEND PROPERTY STRINGS noop interpreter dynarmic ballistic)

set(GPU_BACKEND "auto" CACHE STRING "GPU backend: webgpu|vulkan|metal|d3d12|auto")
set_property(CACHE GPU_BACKEND PROPERTY STRINGS webgpu vulkan metal d3d12 auto)

set(AUDIO_BACKEND "auto" CACHE STRING "Audio backend: webaudio|wasapi|coreaudio|pipewire|auto")
set_property(CACHE AUDIO_BACKEND PROPERTY STRINGS webaudio wasapi coreaudio pipewire auto)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug|Release|RelWithDebInfo" FORCE)
endif()

#
# Language settings. Core is C11 strict, no C++ except dynarmic wrapper.
#
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#
# Strict warnings on the core. No exceptions-based error handling.
#
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
                      -Wmissing-prototypes -Wno-unused-parameter)
endif()

message(STATUS "Vela configuration")
message(STATUS "  CPU_BACKEND:   ${CPU_BACKEND}")
message(STATUS "  GPU_BACKEND:   ${GPU_BACKEND}")
message(STATUS "  AUDIO_BACKEND: ${AUDIO_BACKEND}")
message(STATUS "  Build type:    ${CMAKE_BUILD_TYPE}")

#
# WASM / Emscripten build flags (see docs/DESIGN.md section 20)
#
if(EMSCRIPTEN)
  add_compile_options(-msimd128)
  add_link_options(
    -sWASM=1
    -sWASM_BIGINT=1
    -sSHARED_MEMORY=1
    -sUSE_PTHREADS=1
    -sPTHREAD_POOL_SIZE=8
    -sMAXIMUM_MEMORY=4GB
    -sMEMORY64=1
    "-sEXPORTED_FUNCTIONS=['_emulator_create','_emulator_destroy','_emulator_run','_emulator_step','_cpu_get_reg','_cpu_set_reg','_cpu_get_pc']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','HEAPU8','HEAPU32']"
  )
endif()

add_subdirectory(core)

#
# Optional: tests (Phase 0 ships with a smoke test only)
#
option(SWITCH_EMU_BUILD_TESTS "Build test binaries" ON)
if(SWITCH_EMU_BUILD_TESTS AND NOT EMSCRIPTEN)
  enable_testing()
  add_subdirectory(tests)
endif()
