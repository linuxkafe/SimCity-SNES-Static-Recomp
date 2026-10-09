cmake_minimum_required(VERSION 3.20)

add_executable(simcity-linux
    main.c
    menu.c
    gui_font.c
    settings_ini.c
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
# Settings persistence: guards the shared settings.ini contract, in particular
# that a Linux run does not delete keys owned by the Windows launcher.
add_executable(simcity-settings-ini test_settings_ini.c settings_ini.c)
target_include_directories(simcity-settings-ini PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(simcity-settings-ini PRIVATE
    -Wall -Wextra -Wpedantic)
add_test(NAME simcity-settings-ini COMMAND simcity-settings-ini)

# Pixel format: the core emits 0xAARRGGBB and the SDL texture format must carry
# matching channel masks. A mismatch swaps red and blue and is invisible in a
# headless run because nothing is presented.
add_executable(simcity-pixel-format test_pixel_format.c)
target_link_libraries(simcity-pixel-format PRIVATE ${SDL2_LIBRARIES})
target_include_directories(simcity-pixel-format PRIVATE ${SDL2_INCLUDE_DIRS})
target_compile_options(simcity-pixel-format PRIVATE
    -Wall -Wextra -Wpedantic)
add_test(NAME simcity-pixel-format COMMAND simcity-pixel-format)
set_tests_properties(simcity-pixel-format PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy")
