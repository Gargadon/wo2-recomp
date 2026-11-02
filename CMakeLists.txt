cmake_minimum_required(VERSION 3.25)
project(warriors_orochi_2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(wo2_game_root "${CMAKE_CURRENT_SOURCE_DIR}/../Warriors Orochi 2 (USA) (En,Fr,De)")
set(wo2_tu_directory "${wo2_game_root}/Warriors Orochi 2 Title Update #1")
option(WO2_PREPARE_GAME "Prepare Title Update from original package" ON)

include(generated/rexglue.cmake)

if(WO2_PREPARE_GAME)
    set(WO2_TU_PACKAGE "" CACHE FILEPATH "Original Title Update #1 STFS container")
    if(NOT EXISTS "${WO2_TU_PACKAGE}" OR IS_DIRECTORY "${WO2_TU_PACKAGE}")
        if(EXISTS "D:/Warriors Orochi 2 Title Update #1/Title Update 1 - Media ID 4454AE3B/TU_15KU1V3_000000C000000.0000000000083")
            set(WO2_TU_PACKAGE "D:/Warriors Orochi 2 Title Update #1/Title Update 1 - Media ID 4454AE3B/TU_15KU1V3_000000C000000.0000000000083")
        else()
            message(FATAL_ERROR "Title Update #1 package not found: '${WO2_TU_PACKAGE}'. Set WO2_TU_PACKAGE.")
        endif()
    endif()

    add_executable(wo2_prepare_tu EXCLUDE_FROM_ALL src/prepare_tu.cpp)
    target_link_libraries(wo2_prepare_tu PRIVATE rex::filesystem rex::core)
    if(WIN32)
        target_link_libraries(wo2_prepare_tu PRIVATE ws2_32)
    endif()

    add_custom_target(wo2_prepare_title_update
        COMMAND $<TARGET_FILE:wo2_prepare_tu> --tu "${WO2_TU_PACKAGE}"
            "${wo2_game_root}/default.xex" "${wo2_tu_directory}"
        DEPENDS wo2_prepare_tu
        VERBATIM)
    add_dependencies(warriors_orochi_2_codegen wo2_prepare_title_update)
else()
    foreach(wo2_prepared_file IN ITEMS
            "${wo2_tu_directory}/default.xex"
            "${wo2_tu_directory}/default.xexp")
        if(NOT EXISTS "${wo2_prepared_file}" OR IS_DIRECTORY "${wo2_prepared_file}")
            message(FATAL_ERROR "Prepared Title Update missing: ${wo2_prepared_file}. Run build.ps1 first.")
        endif()
    endforeach()
endif()
if(WIN32)
    add_executable(warriors_orochi_2 WIN32 src/main.cpp)
else()
    add_executable(warriors_orochi_2 src/main.cpp)
endif()
target_sources(warriors_orochi_2 PRIVATE src/event_diagnostics.cpp)
rexglue_setup_target(warriors_orochi_2 GPU_PLUGINS xenos)

if(TARGET imgui)
    target_link_libraries(warriors_orochi_2 PRIVATE imgui)
endif()

# Match the reference project's Release link-time optimization.
foreach(wo2_target IN ITEMS warriors_orochi_2 warriors_orochi_2_recomp)
    target_compile_options(${wo2_target} PRIVATE $<$<CONFIG:Release>:-flto=thin>)
endforeach()
target_link_options(warriors_orochi_2 PRIVATE $<$<CONFIG:Release>:-flto=thin>)
if(WIN32)
    target_link_options(warriors_orochi_2 PRIVATE
        $<$<CONFIG:Release>:-Wl,/OPT:REF> $<$<CONFIG:Release>:-Wl,/OPT:ICF>)
endif()

target_sources(warriors_orochi_2 PRIVATE src/frame_stats.cpp)

# Keep optimized Release builds diagnosable in ETW/WPA.
if(WIN32)
    foreach(wo2_symbol_target IN ITEMS warriors_orochi_2 warriors_orochi_2_recomp)
        target_compile_options(${wo2_symbol_target} PRIVATE
            $<$<CONFIG:Release>:-gcodeview> $<$<CONFIG:Release>:-gline-tables-only>)
    endforeach()
    target_link_options(warriors_orochi_2 PRIVATE
        $<$<CONFIG:Release>:-Wl,/DEBUG:FULL> $<$<CONFIG:Release>:-Wl,/MAP>)
endif()

target_sources(warriors_orochi_2 PRIVATE src/queue_wait.cpp)

target_sources(warriors_orochi_2 PRIVATE src/queue_trace.cpp)

target_sources(warriors_orochi_2 PRIVATE src/video_trace.cpp)

# and the [[midasm_hook]] entries in warriors_orochi_2_config.toml.

# SDK-only rebuilds do not run the executable's POST_BUILD DLL copy.
if(WIN32 AND TARGET rexgpu-xenos)
    add_custom_target(wo2_sync_gpu ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:rexgpu-xenos> $<TARGET_FILE_DIR:warriors_orochi_2>
        DEPENDS rexgpu-xenos warriors_orochi_2
        VERBATIM)
endif()
