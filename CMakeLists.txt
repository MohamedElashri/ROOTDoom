cmake_minimum_required(VERSION 3.24)
project(ROOTDoom VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(ROOTDOOM_BUILD_TESTS "Build ROOTDoom tests" ON)
option(ROOTDOOM_ENABLE_GUI_TESTS "Run tests requiring a working native ROOT display (X11 or macOS Cocoa)" OFF)
option(ROOTDOOM_ENABLE_SANITIZERS "Instrument project C++ code with AddressSanitizer/UBSan" OFF)
set(ROOTDOOM_TEST_IWAD "$ENV{ROOTDOOM_TEST_IWAD}" CACHE FILEPATH "Optional explicitly supplied IWAD for real-engine tests")

find_package(ROOT 6 CONFIG REQUIRED COMPONENTS Core Gui Gpad ASImage RGL)
find_package(OpenGL REQUIRED OPTIONAL_COMPONENTS EGL)
find_package(Threads REQUIRED)

# Match ROOT's ABI when its installation requires a newer standard.
set(CMAKE_CXX_STANDARD 17)
if(ROOT_CXX_STANDARD GREATER CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD ${ROOT_CXX_STANDARD})
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/DoomGeneric.cmake)

function(rootdoom_project_options target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(ROOTDOOM_ENABLE_SANITIZERS)
      target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
      target_link_options(${target} PUBLIC -fsanitize=address,undefined)
    endif()
  elseif(ROOTDOOM_ENABLE_SANITIZERS)
    message(FATAL_ERROR "Sanitizer option currently supports GCC and Clang only")
  endif()
endfunction()

add_library(rootdoom_core STATIC src/Clock.cpp src/DoomRunner.cpp src/FrameMailbox.cpp src/Wad.cpp
  src/InputQueue.cpp src/InputBridge.cpp src/KeyMapping.cpp)
target_include_directories(rootdoom_core PUBLIC include)
target_include_directories(rootdoom_core SYSTEM PUBLIC ${ROOT_INCLUDE_DIRS})
target_include_directories(rootdoom_core PRIVATE "${doomgeneric_SOURCE_DIR}/doomgeneric")
target_link_libraries(rootdoom_core PUBLIC Threads::Threads)
rootdoom_project_options(rootdoom_core)
add_library(rootdoom_geometry STATIC src/GeometryFrame.cpp src/GeometryAssets.cpp)
target_include_directories(rootdoom_geometry PUBLIC include)
target_link_libraries(rootdoom_geometry PUBLIC Threads::Threads)
rootdoom_project_options(rootdoom_geometry)
include(cmake/DoomEngine.cmake)

add_library(rootdoom_render STATIC src/Frame.cpp src/RootImagePresenter.cpp src/GeometryGL.cpp src/RootGeometryPresenter.cpp)
target_include_directories(rootdoom_render PUBLIC include)
target_link_libraries(rootdoom_render PUBLIC rootdoom_geometry ROOT::Core ROOT::Gpad ROOT::ASImage ROOT::RGL OpenGL::GL)
rootdoom_project_options(rootdoom_render)

add_library(rootdoom_runtime SHARED src/RootDoomWindow.cpp src/Runtime.cpp)
set_target_properties(rootdoom_runtime PROPERTIES OUTPUT_NAME rootdoom)
target_include_directories(rootdoom_runtime PUBLIC include)
target_include_directories(rootdoom_runtime PRIVATE "${doomgeneric_SOURCE_DIR}/doomgeneric")
target_link_libraries(rootdoom_runtime PRIVATE rootdoom_engine rootdoom_core rootdoom_render ROOT::Gui Threads::Threads)
rootdoom_project_options(rootdoom_runtime)

add_executable(rootdoom src/main.cpp)
target_link_libraries(rootdoom PRIVATE rootdoom_runtime)
rootdoom_project_options(rootdoom)

if(ROOTDOOM_BUILD_TESTS)
  enable_testing()
  add_test(NAME cli_help COMMAND rootdoom --help)
  set_tests_properties(cli_help PROPERTIES PASS_REGULAR_EXPRESSION "self-test-gui")
  add_test(NAME cli_unknown_option COMMAND rootdoom --unknown-option)
  set_tests_properties(cli_unknown_option PROPERTIES WILL_FAIL TRUE)
  add_test(NAME cli_no_iwad COMMAND rootdoom)
  add_test(NAME cli_missing_iwad COMMAND rootdoom --iwad /nonexistent/rootdoom-test/doom1.wad)
  set_tests_properties(cli_no_iwad cli_missing_iwad PROPERTIES WILL_FAIL TRUE)
  add_test(NAME root_runtime COMMAND ROOT::root -l -n -b -q "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_root_runtime.C")
  set_tests_properties(root_runtime PROPERTIES
    TIMEOUT 30
    PASS_REGULAR_EXPRESSION "ROOT_RUNTIME_OK"
    FAIL_REGULAR_EXPRESSION "Error in;fatal error:;Failed to load module;ROOT_RUNTIME_FAILED"
  )
  add_test(NAME root_library_entry COMMAND ROOT::root -l -n -b -q
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_root_entry.C(\"$<TARGET_FILE:rootdoom_runtime>\")")
  set_tests_properties(root_library_entry PROPERTIES TIMEOUT 20
    PASS_REGULAR_EXPRESSION "ROOT_ENTRY_OK" FAIL_REGULAR_EXPRESSION "ROOT_ENTRY_FAILED;Error in")
  add_executable(test_presenter tests/test_presenter.cpp)
  target_link_libraries(test_presenter PRIVATE rootdoom_render)
  rootdoom_project_options(test_presenter)
  add_test(NAME presenter_pixels COMMAND test_presenter)
  set_tests_properties(presenter_pixels PROPERTIES TIMEOUT 30)
  add_test(NAME pattern_batch COMMAND rootdoom --batch-pattern --duration 2
    --screenshot pattern-batch.png --metrics pattern-batch.json)
  set_tests_properties(pattern_batch PROPERTIES TIMEOUT 15)
  add_executable(test_runtime_core tests/test_runtime_core.cpp)
  target_link_libraries(test_runtime_core PRIVATE rootdoom_core)
  target_include_directories(test_runtime_core PRIVATE "${doomgeneric_SOURCE_DIR}/doomgeneric")
  rootdoom_project_options(test_runtime_core)
  add_test(NAME runtime_core COMMAND test_runtime_core)
  set_tests_properties(runtime_core PROPERTIES TIMEOUT 30)
  add_executable(test_geometry tests/test_geometry.cpp)
  target_link_libraries(test_geometry PRIVATE rootdoom_geometry)
  rootdoom_project_options(test_geometry)
  add_test(NAME geometry_core COMMAND test_geometry)
  if(TARGET OpenGL::EGL)
    add_executable(test_geometry_pixels tests/test_geometry_pixels.cpp)
    target_link_libraries(test_geometry_pixels PRIVATE rootdoom_render OpenGL::EGL)
    rootdoom_project_options(test_geometry_pixels)
    add_test(NAME geometry_pixels COMMAND test_geometry_pixels)
    set_tests_properties(geometry_pixels PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77
      ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
  endif()
  add_test(NAME worker_batch COMMAND rootdoom --batch-worker --duration 2
    --screenshot worker-batch.png --metrics worker-batch.json)
  set_tests_properties(worker_batch PROPERTIES TIMEOUT 15)
  add_executable(test_wad tests/test_wad.cpp)
  target_link_libraries(test_wad PRIVATE rootdoom_engine)
  rootdoom_project_options(test_wad)
  add_test(NAME wad_validation COMMAND test_wad)
  add_executable(test_engine_lifecycle tests/test_engine_lifecycle.cpp)
  target_link_libraries(test_engine_lifecycle PRIVATE rootdoom_engine)
  rootdoom_project_options(test_engine_lifecycle)
  add_test(NAME engine_startup_failure COMMAND test_engine_lifecycle failure)
  add_test(NAME engine_early_stop COMMAND test_engine_lifecycle stop)
  set_tests_properties(engine_startup_failure engine_early_stop PROPERTIES
    TIMEOUT 15 PASS_REGULAR_EXPRESSION "ENGINE_HOST_SURVIVED")
  if(ROOTDOOM_TEST_IWAD)
    add_executable(test_geometry_scene tests/test_geometry_scene.cpp)
    target_link_libraries(test_geometry_scene PRIVATE rootdoom_engine rootdoom_render)
    target_include_directories(test_geometry_scene PRIVATE "${doomgeneric_SOURCE_DIR}/doomgeneric")
    rootdoom_project_options(test_geometry_scene)
    add_test(NAME geometry_scene COMMAND test_geometry_scene "${ROOTDOOM_TEST_IWAD}")
    set_tests_properties(geometry_scene PROPERTIES TIMEOUT 30 LABELS iwad)
    if(TARGET OpenGL::EGL)
      target_compile_definitions(test_geometry_scene PRIVATE ROOTDOOM_TEST_EGL)
      target_link_libraries(test_geometry_scene PRIVATE OpenGL::EGL)
      add_test(NAME geometry_level_pixels COMMAND test_geometry_scene "${ROOTDOOM_TEST_IWAD}" render)
      set_tests_properties(geometry_level_pixels PROPERTIES TIMEOUT 30 LABELS iwad SKIP_RETURN_CODE 77
        ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
    endif()
    add_executable(test_gameplay tests/test_gameplay.cpp)
    target_link_libraries(test_gameplay PRIVATE rootdoom_engine)
    target_include_directories(test_gameplay PRIVATE "${doomgeneric_SOURCE_DIR}/doomgeneric")
    rootdoom_project_options(test_gameplay)
    add_test(NAME doom_real_input COMMAND test_gameplay "${ROOTDOOM_TEST_IWAD}" movement)
    add_test(NAME doom_menu_quit COMMAND test_gameplay "${ROOTDOOM_TEST_IWAD}" quit)
    set_tests_properties(doom_real_input doom_menu_quit PROPERTIES
      TIMEOUT 20 LABELS iwad PASS_REGULAR_EXPRESSION "GAMEPLAY_OK")
    add_test(NAME doom_game_batch COMMAND rootdoom --batch-game --iwad "${ROOTDOOM_TEST_IWAD}"
      --duration 3 --data-dir game-smoke-data --screenshot doom-game-smoke.png
      --metrics doom-game-smoke.json -- -warp 1 1 -nomonsters)
    set_tests_properties(doom_game_batch PROPERTIES TIMEOUT 20 LABELS iwad)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      find_library(ROOTDOOM_TEST_CRYPTO_LIBRARY NAMES crypto)
      if(ROOTDOOM_TEST_CRYPTO_LIBRARY)
        # ROOT loads OpenSSL before our DSO. Reproduce that symbol lookup order
        # without requiring a display to catch collisions in vendor C symbols.
        add_test(NAME doom_preloaded_crypto COMMAND rootdoom --batch-game
          --iwad "${ROOTDOOM_TEST_IWAD}" --duration 2
          --data-dir crypto-smoke-data -- -warp 1 1 -nomonsters)
        set_tests_properties(doom_preloaded_crypto PROPERTIES TIMEOUT 20 LABELS iwad
          ENVIRONMENT "LD_PRELOAD=${ROOTDOOM_TEST_CRYPTO_LIBRARY}")
      endif()
    endif()
  endif()
  if(ROOTDOOM_ENABLE_GUI_TESTS)
    add_executable(test_geometry_native tests/test_geometry_native.cpp)
    target_link_libraries(test_geometry_native PRIVATE rootdoom_runtime rootdoom_render)
    rootdoom_project_options(test_geometry_native)
    add_test(NAME native_geometry_colors COMMAND test_geometry_native)
    set_tests_properties(native_geometry_colors PROPERTIES TIMEOUT 20 LABELS gui
      PASS_REGULAR_EXPRESSION "ROOT_GEOMETRY_NATIVE_OK")
    add_test(NAME native_gui_baseline COMMAND rootdoom --self-test-gui)
    set_tests_properties(native_gui_baseline PROPERTIES TIMEOUT 15 LABELS gui)
    add_test(NAME native_worker_smoke COMMAND rootdoom --worker-pattern --duration 2
      --screenshot native-worker-smoke.png --metrics native-worker-smoke.json)
    set_tests_properties(native_worker_smoke PROPERTIES TIMEOUT 15 LABELS gui)
    if(ROOTDOOM_TEST_IWAD)
      add_test(NAME native_default_renderer COMMAND rootdoom --iwad "${ROOTDOOM_TEST_IWAD}"
        --duration 3 --data-dir native-geometry-data --screenshot native-default.png --metrics native-default.json
        -- -warp 1 1 -nomonsters)
      set_tests_properties(native_default_renderer PROPERTIES TIMEOUT 20 LABELS "gui;iwad"
        PASS_REGULAR_EXPRESSION "ROOT geometry: world frames=[1-9]")
      add_test(NAME native_geometry_interpreter COMMAND ROOT::root -l -n -q
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_root_native.C(\"$<TARGET_FILE:rootdoom_runtime>\",\"${ROOTDOOM_TEST_IWAD}\",\"rootgl\")")
      set_tests_properties(native_geometry_interpreter PROPERTIES TIMEOUT 20 LABELS "gui;iwad"
        PASS_REGULAR_EXPRESSION "ROOT_NATIVE_ENTRY_OK" FAIL_REGULAR_EXPRESSION "ROOT_NATIVE_ENTRY_FAILED")
      add_test(NAME native_classic_smoke COMMAND rootdoom --renderer classic --iwad "${ROOTDOOM_TEST_IWAD}"
        --duration 2 --data-dir native-classic-data --metrics native-classic.json -- -warp 1 1 -nomonsters)
      set_tests_properties(native_classic_smoke PROPERTIES TIMEOUT 20 LABELS "gui;iwad"
        PASS_REGULAR_EXPRESSION "Native Doom: frames=")
      add_test(NAME native_root_interpreter COMMAND ROOT::root -l -n -q
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_root_native.C(\"$<TARGET_FILE:rootdoom_runtime>\",\"${ROOTDOOM_TEST_IWAD}\")")
      set_tests_properties(native_root_interpreter PROPERTIES TIMEOUT 20 LABELS "gui;iwad"
        PASS_REGULAR_EXPRESSION "ROOT_NATIVE_ENTRY_OK" FAIL_REGULAR_EXPRESSION "ROOT_NATIVE_ENTRY_FAILED")
    endif()
  endif()
endif()

message(STATUS "ROOTDoom: ROOT ${ROOT_VERSION}, C++${CMAKE_CXX_STANDARD}")
message(STATUS "Doom source: ${doomgeneric_SOURCE_DIR} (${ROOTDOOM_DOOMGENERIC_REVISION})")
