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
# Parent CMakeLists.txt sets SDL2_FOUND or SDL3_FOUND
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
else()
    message(FATAL_ERROR "Neither SDL2 nor SDL3 found")
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