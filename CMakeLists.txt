cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
project(SimCitySNESStaticRecomp LANGUAGES C CXX)

# An unoptimised build runs the static-recomp core roughly three times slower
# than the release build (measured: 58 ms -> 20 ms per frame on the build
# host), which is enough to miss the 16.6 ms NTSC frame budget on its own.
# Default to Release so a plain `cmake ..` produces a playable binary.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not set; defaulting to Release")
endif()

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # Keep source/build-machine directory names out of diagnostics embedded in
    # portable release binaries. These mappings do not affect runtime paths.
    add_compile_options(
        "/experimental:deterministic"
        "/pathmap:${CMAKE_SOURCE_DIR}=source"
        "/pathmap:${CMAKE_BINARY_DIR}=build"
    )
    # Keep release binaries byte-for-byte stable when the source and MSVC
    # toolchain are unchanged. Antivirus reputation is hash-based, so changing
    # the PE timestamp on every clean build needlessly creates a new unknown
    # executable even when no code changed.
    add_compile_options($<$<CONFIG:Release>:/Brepro>)
    add_link_options($<$<CONFIG:Release>:/Brepro>)
endif()

add_subdirectory(static-recomp)

add_executable(simcity-widescreen-probe
    tools/widescreen/widescreen_probe.c)
target_link_libraries(simcity-widescreen-probe PRIVATE simcity-static-recomp)
target_include_directories(simcity-widescreen-probe PRIVATE
    "${CMAKE_SOURCE_DIR}/static-recomp/include")
target_compile_definitions(simcity-widescreen-probe PRIVATE
    _CRT_SECURE_NO_WARNINGS)
if(MSVC)
    target_compile_options(simcity-widescreen-probe PRIVATE /W4 /WX)
else()
    target_compile_options(simcity-widescreen-probe PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()

if(WIN32)
    add_subdirectory(frontend/windows)
elseif(UNIX AND NOT APPLE)
    # The frontend is written against the SDL2 API and is not portable to SDL3
    # without a real port (SDL3's 2D renderer differs), so SDL2 is required
    # rather than merely preferred. The previous SDL3 fallbacks reached
    # add_subdirectory() without ever populating SDL2_INCLUDE_DIRS, so they
    # configured cleanly and then failed with "SDL.h: No such file or
    # directory" inside the compiler, naming neither the missing package nor
    # the option to skip the frontend. Fail here instead, with instructions.
    set(BUILD_LINUX_FRONTEND ON CACHE BOOL "Build the Linux SDL2 frontend")
    if(BUILD_LINUX_FRONTEND)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 sdl2)
        if(NOT SDL2_FOUND)
            message(FATAL_ERROR
                "SDL2 development files not found, so the Linux frontend "
                "cannot be built.\n"
                "  Debian/Ubuntu:  sudo apt install libsdl2-dev\n"
                "  Fedora:         sudo dnf install SDL2-devel\n"
                "  Arch:           sudo pacman -S sdl2\n"
                "To build only the portable core and headless test:\n"
                "  cmake -DBUILD_LINUX_FRONTEND=OFF")
        endif()
        message(STATUS "Linux frontend: using system SDL2 ${SDL2_VERSION}")
        add_subdirectory(frontend/linux)
    else()
        message(STATUS "Linux frontend disabled (BUILD_LINUX_FRONTEND=OFF)")
    endif()
else()
    message(STATUS "Windows/frontend omitted on this host; building the portable Full Static core.")
endif()

install(TARGETS simcity-static-recomp ARCHIVE DESTINATION lib
    COMPONENT Development)
install(FILES static-recomp/include/simcity_static_recomp.h DESTINATION include
    COMPONENT Development)
install(FILES README-FIRST.txt VERSION.txt ROM-REQUIREMENTS.txt
    "Wide Screen - 1.4.0.txt" "Audio - 1.4.0.txt" DESTINATION .
    COMPONENT Runtime)
install(FILES Rom/readme.txt DESTINATION Rom
    COMPONENT Runtime)
