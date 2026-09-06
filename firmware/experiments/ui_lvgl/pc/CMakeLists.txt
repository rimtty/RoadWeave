cmake_minimum_required(VERSION 3.24)
project(roadweave_ui_pc LANGUAGES C CXX)
# Some CMake/MSVC combinations misdecode Japanese /showIncludes as ANSI even
# though cl emits UTF-8. Correct Ninja's prefix so header edits trigger rebuilds.
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/include_probe.c" "#include <stdio.h>\n")
    execute_process(COMMAND "${CMAKE_C_COMPILER}" /nologo /showIncludes /Zs
        "${CMAKE_CURRENT_BINARY_DIR}/include_probe.c"
        OUTPUT_VARIABLE include_probe ERROR_VARIABLE include_probe_error ENCODING UTF-8)
    if(include_probe MATCHES "(^|\n)([^\r\n]*:  )[A-Za-z]:[^\r\n]*stdio.h")
        set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_2}")
        set(CMAKE_C_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_2}")
        set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "${CMAKE_MATCH_2}")
    endif()
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
include(FetchContent)
include(CTest)

# Commits corresponding to LVGL v9.5.0 and SDL release-2.32.10.
FetchContent_Declare(lvgl GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG 85aa60d18b3d5e5588d7b247abf90198f07c8a63)
FetchContent_Declare(sdl2 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG 5d249570393f7a37e037abf22cd6012a4cc56a71)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
FetchContent_MakeAvailable(sdl2 lvgl)
target_link_libraries(lvgl PUBLIC SDL2::SDL2-static)
if(MSVC)
    target_compile_definitions(lvgl PUBLIC _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES)
    target_compile_options(lvgl PRIVATE /utf-8)
endif()

add_library(roadweave_ui STATIC ../main/ui_screens.c ../main/ui_simulation.c
    ../../../components/position/position.c)
target_include_directories(roadweave_ui PUBLIC ../main ../../../components/position/include)
target_link_libraries(roadweave_ui PUBLIC lvgl::lvgl)
if(NOT MSVC)
    target_link_libraries(roadweave_ui PUBLIC m)
endif()
add_executable(roadweave_ui_sim main.c scenarios.c capture.c)
target_link_libraries(roadweave_ui_sim PRIVATE roadweave_ui)
target_compile_definitions(roadweave_ui_sim PRIVATE SDL_MAIN_HANDLED)
if(MSVC)
    target_compile_options(roadweave_ui PRIVATE /W4 /utf-8)
    target_compile_options(roadweave_ui_sim PRIVATE /W4 /utf-8)
endif()

set(RW_SCENARIOS group radar convoy empty max_group max_convoy stale
    radar_east radar_far rx tx busy link_down)
foreach(scenario IN LISTS RW_SCENARIOS)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/results/${scenario}")
    add_test(NAME image_${scenario} COMMAND ${CMAKE_COMMAND}
        "-DEXE=$<TARGET_FILE:roadweave_ui_sim>"
        "-DSCENARIO=${scenario}"
        "-DBASELINE=${CMAKE_CURRENT_SOURCE_DIR}/baselines/${scenario}.png"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/compare.cmake")
    set_tests_properties(image_${scenario} PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/results/${scenario}" TIMEOUT 20)
endforeach()
add_test(NAME interactions COMMAND roadweave_ui_sim --test-input)
set_tests_properties(interactions PROPERTIES TIMEOUT 20)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/results/window"
    "${CMAKE_CURRENT_BINARY_DIR}/results/compare_contract")
add_test(NAME sdl_window COMMAND roadweave_ui_sim --test-window)
set_tests_properties(sdl_window PROPERTIES TIMEOUT 20
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/results/window")
add_test(NAME compare_contract COMMAND ${CMAKE_COMMAND}
    "-DEXE=$<TARGET_FILE:roadweave_ui_sim>"
    "-DBASELINE=${CMAKE_CURRENT_SOURCE_DIR}/baselines/group.png"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/negative.cmake")
set_tests_properties(compare_contract PROPERTIES TIMEOUT 20
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/results/compare_contract")
