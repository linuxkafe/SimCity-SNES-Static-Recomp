cmake_minimum_required(VERSION 3.20)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
project(SimCitySNESStaticRecomp LANGUAGES C CXX)

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
    # Linux frontend: prefer system SDL2, fall back to fetched SDL3.
    set(BUILD_LINUX_FRONTEND ON CACHE BOOL "Build the Linux SDL2/SDL3 frontend")
    if(BUILD_LINUX_FRONTEND)
        find_package(PkgConfig QUIET)
        pkg_check_modules(SDL2 QUIET sdl2)
        if(SDL2_FOUND)
            message(STATUS "Linux frontend: using system SDL2 ${SDL2_VERSION}")
            add_subdirectory(frontend/linux)
        else()
            # SDL3 may be installed without pkg-config (e.g. Steam Deck).
            # Try config mode first, then fetch from source.
            find_package(SDL3 QUIET CONFIG)
            if(TARGET SDL3::SDL3)
                message(STATUS "Linux frontend: using system SDL3 (config)")
                set(SDL3_FOUND TRUE)
                add_subdirectory(frontend/linux)
            else()
                message(STATUS
                    "SDL2/SDL3 not found via pkg-config/config; fetching SDL3 from source")
                include(FetchContent)
                set(SDL_SHARED OFF CACHE BOOL "" FORCE)
                set(SDL_STATIC ON CACHE BOOL "" FORCE)
                set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
                set(SDL_TESTS OFF CACHE BOOL "" FORCE)
                set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
                set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
                FetchContent_Declare(
                    SDL3
                    URL "https://github.com/libsdl-org/SDL/releases/download/release-3.2.18/SDL3-3.2.18.tar.gz"
                    URL_HASH "SHA256=1a775bde924397a8e0c08bfda198926c17be859d0288ad0dec1dea1b2ee04f8f"
                    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                )
                FetchContent_MakeAvailable(SDL3)
                add_subdirectory(frontend/linux)
                message(STATUS "Linux frontend enabled (SDL3 fetched)")
            endif()
        endif()
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
