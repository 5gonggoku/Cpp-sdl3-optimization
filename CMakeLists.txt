cmake_minimum_required(VERSION 3.21)
project(MyGame LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

option(GAME_ASAN "Build with AddressSanitizer (MSVC /fsanitize=address)" OFF)
if(GAME_ASAN AND MSVC)
  add_compile_options(/fsanitize=address)
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE DATA_SOURCES CONFIGURE_DEPENDS src/data/*.cpp)
file(GLOB_RECURSE SIM_SOURCES CONFIGURE_DEPENDS src/gameplay/*.cpp)
list(REMOVE_ITEM SOURCES ${DATA_SOURCES} ${SIM_SOURCES})

add_library(gamedata STATIC ${DATA_SOURCES})
target_include_directories(gamedata PUBLIC src)

add_library(gamesim STATIC ${SIM_SOURCES})
target_include_directories(gamesim PUBLIC src)
target_link_libraries(gamesim PUBLIC gamedata)

add_executable(game ${SOURCES})
target_include_directories(game PRIVATE src)
target_link_libraries(game PRIVATE gamesim gamedata SDL3::SDL3 imgui::imgui)

enable_testing()
add_executable(json_tests tests/json_tests.cpp)
target_link_libraries(json_tests PRIVATE gamedata)
target_compile_definitions(json_tests PRIVATE GAME_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
add_test(NAME json_tests COMMAND json_tests)

add_executable(world_tests tests/world_tests.cpp)
target_link_libraries(world_tests PRIVATE gamesim gamedata)
target_compile_definitions(world_tests PRIVATE GAME_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
add_test(NAME world_tests COMMAND world_tests)

add_executable(pool_tests tests/pool_tests.cpp)
target_include_directories(pool_tests PRIVATE src)
add_test(NAME pool_tests COMMAND pool_tests)

add_executable(grid_tests tests/grid_tests.cpp)
target_include_directories(grid_tests PRIVATE src)
add_test(NAME grid_tests COMMAND grid_tests)

find_package(Git QUIET)
set(GAME_GIT_COMMIT "unknown")
if(GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GAME_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  execute_process(COMMAND ${GIT_EXECUTABLE} status --porcelain
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  OUTPUT_VARIABLE GAME_GIT_STATUS OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(GAME_GIT_STATUS)
    set(GAME_GIT_COMMIT "${GAME_GIT_COMMIT}-dirty")
  endif()
endif()
target_compile_definitions(game PRIVATE
  GAME_GIT_COMMIT="${GAME_GIT_COMMIT}"
  GAME_BUILD_TYPE="${CMAKE_BUILD_TYPE}")

if(MSVC)
  foreach(t game gamedata gamesim json_tests world_tests pool_tests grid_tests)
    target_compile_options(${t} PRIVATE /W4 /permissive- /utf-8)
  endforeach()
endif()

if(GAME_ASAN AND MSVC)
  set(ASAN_DLL "$ENV{VCToolsInstallDir}/bin/Hostx64/x64/clang_rt.asan_dynamic-x86_64.dll")
  if(NOT EXISTS "${ASAN_DLL}")
    message(FATAL_ERROR "ASan 런타임 DLL을 찾을 수 없음: ${ASAN_DLL} (x64 Developer 환경에서 configure하고, VS의 'C++ AddressSanitizer' 구성 요소를 확인할 것)")
  endif()
  add_executable(pool_asan_probe tests/pool_asan_probe.cpp)
  target_include_directories(pool_asan_probe PRIVATE src)
  target_compile_options(pool_asan_probe PRIVATE /W4 /permissive- /utf-8)
  add_test(NAME pool_asan_probe_released COMMAND pool_asan_probe released)
  add_test(NAME pool_asan_probe_unused COMMAND pool_asan_probe unused)
  set_tests_properties(pool_asan_probe_released pool_asan_probe_unused PROPERTIES
    PASS_REGULAR_EXPRESSION "AddressSanitizer: use-after-poison")

  add_executable(pool_asan_probe_nopoison tests/pool_asan_probe.cpp)
  target_include_directories(pool_asan_probe_nopoison PRIVATE src)
  target_compile_definitions(pool_asan_probe_nopoison PRIVATE POOL_NO_ASAN_POISON)
  target_compile_options(pool_asan_probe_nopoison PRIVATE /W4 /permissive- /utf-8)
  add_test(NAME pool_asan_probe_nopoison_released COMMAND pool_asan_probe_nopoison released)
  set_tests_properties(pool_asan_probe_nopoison_released PROPERTIES
    PASS_REGULAR_EXPRESSION "ASan이 잡지 못함")

  add_library(gamedata_fuzz STATIC ${DATA_SOURCES})
  target_include_directories(gamedata_fuzz PUBLIC src)
  target_compile_options(gamedata_fuzz PRIVATE /fsanitize=fuzzer /W4 /permissive- /utf-8)
  add_executable(json_fuzz tests/json_fuzz.cpp)
  target_compile_options(json_fuzz PRIVATE /fsanitize=fuzzer /W4 /permissive- /utf-8)
  target_link_libraries(json_fuzz PRIVATE gamedata_fuzz)

  foreach(t game json_tests world_tests pool_tests grid_tests pool_asan_probe pool_asan_probe_nopoison json_fuzz)
    add_custom_command(TARGET ${t} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ASAN_DLL}" "$<TARGET_FILE_DIR:${t}>")
  endforeach()
endif()
