cmake_minimum_required(VERSION 3.20)
project(BN6FalzarRecomp C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GBARECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/gbarecomp"
    CACHE PATH "Path to the gbarecomp checkout")
set(RECOMP_UI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/recomp-ui"
    CACHE PATH "Path to the recomp-ui checkout")
set(GBARECOMP_RUNTIME_UI_ROOT "${RECOMP_UI_ROOT}"
    CACHE PATH "recomp-ui root used by the live GBA runtime adapter")
# The recompiled GBA BIOS is generated locally (tools/regen.sh) and kept out
# of the gbarecomp submodule.
set(GBARECOMP_GENERATED_BIOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated_bios"
    CACHE PATH "Directory containing locally generated BIOS recompilation sources")

if(NOT EXISTS "${GBARECOMP_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR
        "GBARECOMP_ROOT is not a gbarecomp checkout:\n  ${GBARECOMP_ROOT}\n"
        "Run `git submodule update --init --recursive`.")
endif()

add_subdirectory("${GBARECOMP_ROOT}" gbarecomp_build EXCLUDE_FROM_ALL)

file(GLOB GAME_GENERATED_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/*.cpp")
if(NOT GAME_GENERATED_SOURCES)
    message(WARNING "generated/ is empty: run tools/regen.sh first.")
endif()

add_executable(BN6FalzarRecomp
    src/main.cpp
    ${GAME_GENERATED_SOURCES})

# Keep the very large generated guest translation units at a bounded
# optimization/debug-info cost.
set(GAME_RECOMPILED_SOURCES ${GAME_GENERATED_SOURCES})
list(FILTER GAME_RECOMPILED_SOURCES INCLUDE REGEX "/recompiled_[0-9]+\\.cpp$")
if(GAME_RECOMPILED_SOURCES)
    set_source_files_properties(${GAME_RECOMPILED_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-O1;-g0")
endif()

target_include_directories(BN6FalzarRecomp PRIVATE
    "${GBARECOMP_ROOT}/src/armv4t"
    "${GBARECOMP_ROOT}/src/gba"
    "${GBARECOMP_ROOT}/src/runtime"
    "${GBARECOMP_ROOT}/src/debug"
    src)

target_link_libraries(BN6FalzarRecomp PRIVATE
    "-Wl,--start-group"
    gbarecomp_runtime
    gbarecomp_debug
    gbarecomp_gba
    gbarecomp_armv4t
    "-Wl,--end-group")

target_compile_definitions(BN6FalzarRecomp PRIVATE
    GBARECOMP_DEFAULT_DEBUG_PORT=19866
    GBARECOMP_WINDOW_TITLE="BN6FalzarRecomp"
    GBARECOMP_DEFAULT_GAME_CONFIG="game.toml")

# recomp-ui pre-boot launcher (Dear ImGui), isolated in its own static lib so
# its compile flags never touch the generated translation units.
option(GBAGAME_RECOMP_UI "Build the recomp-ui pre-boot launcher" ON)
if(GBAGAME_RECOMP_UI)
    include(${RECOMP_UI_ROOT}/recomp_ui.cmake)
    add_library(game_launcher_ui STATIC src/game_launcher_boot.cpp)
    target_include_directories(game_launcher_ui PRIVATE
        "${GBARECOMP_ROOT}/src/runtime"
        "${GBARECOMP_ROOT}/src/gba"
        src)
    recomp_target_launcher_ui(game_launcher_ui CONSOLE gba)
    find_package(SDL2 REQUIRED)
    target_link_libraries(game_launcher_ui PUBLIC SDL2::SDL2)
    target_link_libraries(BN6FalzarRecomp PRIVATE game_launcher_ui)
    set_property(SOURCE src/main.cpp APPEND PROPERTY COMPILE_DEFINITIONS GBAGAME_RECOMP_UI)
endif()
