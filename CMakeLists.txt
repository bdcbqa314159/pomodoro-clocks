cmake_minimum_required(VERSION 3.21)
project(pomodoro VERSION 0.1.0 LANGUAGES CXX)

# CMake's stock prefix is /usr/local, which needs root. Default to somewhere the
# user already owns; -DCMAKE_INSTALL_PREFIX=... still overrides this.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(DEFINED ENV{HOME})
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Install prefix" FORCE)
  elseif(DEFINED ENV{LOCALAPPDATA})
    set(CMAKE_INSTALL_PREFIX "$ENV{LOCALAPPDATA}/Programs/pomodoro" CACHE PATH "Install prefix" FORCE)
  endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Pure logic, no I/O — so the tests can link it without dragging in a terminal.
add_library(pomodoro_core src/pomodoro.cpp)
target_compile_features(pomodoro_core PUBLIC cxx_std_20)
target_include_directories(pomodoro_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

include(FetchContent)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0
  GIT_SHALLOW TRUE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# Off, or `cmake --install` scatters FTXUI's headers, static libs, pkgconfig and
# CMake package files across the prefix. We ship one executable.
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ftxui)

# config_io is compiled into the app and the tests directly rather than into
# pomodoro_core — the core stays I/O-free, and the parser still gets tested.
add_executable(pomodoro src/main.cpp src/config_io.cpp)
# ftxui::component transitively brings dom + screen; naming all three makes the
# linker warn about duplicates.
target_link_libraries(pomodoro PRIVATE pomodoro_core ftxui::component)

include(GNUInstallDirs)
install(TARGETS pomodoro RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

option(POMODORO_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
if(POMODORO_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # MSVC
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)          # same reason as FTXUI above
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(pomodoro_tests tests/pomodoro_test.cpp tests/config_io_test.cpp src/config_io.cpp)
  target_link_libraries(pomodoro_tests PRIVATE pomodoro_core GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(pomodoro_tests)
endif()
