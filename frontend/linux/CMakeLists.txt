cmake_minimum_required(VERSION 3.20)

add_executable(simcity-linux
    main.c
)

target_include_directories(simcity-linux PRIVATE
    "${CMAKE_SOURCE_DIR}/static-recomp/include"
    "${CMAKE_SOURCE_DIR}/static-recomp/internal/include"
    "${CMAKE_SOURCE_DIR}/static-recomp/generated"
)

# Link SDL2 or SDL3 depending on what's available
# Check parent CMakeLists.txt variables
if(SDL3_FOUND)
    target_link_libraries(simcity-linux PRIVATE
        simcity-static-recomp
        SDL3::SDL3
    )
    target_compile_definitions(simcity-linux PRIVATE
        SIMCITY_PLATFORM_LINUX
        SIMCITY_USE_SDL3
    )
    message(STATUS "Using SDL3 with Vulkan support")
elseif(TARGET SDL3::SDL3)
    target_link_libraries(simcity-linux PRIVATE
        simcity-static-recomp
        SDL3::SDL3
    )
    target_compile_definitions(simcity-linux PRIVATE
        SIMCITY_PLATFORM_LINUX
        SIMCITY_USE_SDL3
    )
    message(STATUS "Using SDL3 with Vulkan support")
elseif(SDL2_FOUND)
    target_link_libraries(simcity-linux PRIVATE
        simcity-static-recomp
        ${SDL2_LIBRARIES}
    )
    target_include_directories(simcity-linux PRIVATE ${SDL2_INCLUDE_DIRS})
    target_compile_options(simcity-linux PRIVATE ${SDL2_CFLAGS_OTHER})
    target_compile_definitions(simcity-linux PRIVATE
        SIMCITY_PLATFORM_LINUX
    )
    message(STATUS "Using SDL2")
elseif(TARGET SDL2::SDL2)
    target_link_libraries(simcity-linux PRIVATE
        simcity-static-recomp
        SDL2::SDL2
    )
    target_compile_definitions(simcity-linux PRIVATE
        SIMCITY_PLATFORM_LINUX
    )
    message(STATUS "Using SDL2")
else()
    # Fallback: try to find SDL3 or SDL2 directly (works on Steam Deck without pkg-config)
    find_library(SDL3_LIB SDL3 PATHS /usr/lib /usr/lib64 /usr/local/lib)
    if(SDL3_LIB)
        target_link_libraries(simcity-linux PRIVATE
            simcity-static-recomp
            ${SDL3_LIB}
        )
        target_compile_definitions(simcity-linux PRIVATE
            SIMCITY_PLATFORM_LINUX
            SIMCITY_USE_SDL3
        )
        message(STATUS "Using SDL3 (direct link) - Vulkan support available")
    else()
        find_library(SDL2_LIB SDL2 PATHS /usr/lib /usr/lib64 /usr/local/lib)
        if(SDL2_LIB)
            target_link_libraries(simcity-linux PRIVATE
                simcity-static-recomp
                ${SDL2_LIB}
            )
            target_compile_definitions(simcity-linux PRIVATE
                SIMCITY_PLATFORM_LINUX
            )
            message(STATUS "Using SDL2 (direct link)")
        else()
            message(FATAL_ERROR "Neither SDL2 nor SDL3 found")
        endif()
    endif()
endif()

# Add SDL3 include directories for fetched SDL3
if(TARGET SDL3::SDL3-static)
    target_include_directories(simcity-linux PRIVATE
        ${SDL3_SOURCE_DIR}/include
    )
endif()

target_compile_options(simcity-linux PRIVATE
    -Wall -Wextra -Wpedantic
)

# Headless test binary — no SDL dependency. Useful for CI and machines
# without a display server. The ROM path is passed as the first argument
# and the optional frame count as the second (default 300).
add_executable(headless-test
    headless-test.c
)

target_include_directories(headless-test PRIVATE
    "${CMAKE_SOURCE_DIR}/static-recomp/include"
    "${CMAKE_SOURCE_DIR}/static-recomp/internal/include"
    "${CMAKE_SOURCE_DIR}/static-recomp/generated"
)

target_link_libraries(headless-test PRIVATE
    simcity-static-recomp
)

target_compile_options(headless-test PRIVATE
    -Wall -Wextra -Wpedantic
)

target_compile_definitions(headless-test PRIVATE
    SIMCITY_PLATFORM_LINUX
)