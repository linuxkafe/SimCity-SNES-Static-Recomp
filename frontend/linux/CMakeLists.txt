cmake_minimum_required(VERSION 3.20)

add_executable(simcity-linux
    main.c
)

target_include_directories(simcity-linux PRIVATE
    "${CMAKE_SOURCE_DIR}/static-recomp/include"
    "${CMAKE_SOURCE_DIR}/static-recomp/internal/include"
    "${CMAKE_SOURCE_DIR}/static-recomp/generated"
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(simcity-linux PRIVATE
    simcity-static-recomp
    ${SDL2_LIBRARIES}
)

target_compile_options(simcity-linux PRIVATE
    -Wall -Wextra -Wpedantic
    ${SDL2_CFLAGS_OTHER}
)

target_compile_definitions(simcity-linux PRIVATE
    SIMCITY_PLATFORM_LINUX
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