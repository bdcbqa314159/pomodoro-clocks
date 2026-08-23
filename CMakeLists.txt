cmake_minimum_required(VERSION 3.21)
project(pomodoro VERSION 0.1.0 LANGUAGES CXX)

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

# ponytail: no app target yet — the FTXUI dependency lands with milestone 2,
# not before. A fast configure while you're iterating on the core is worth more.

option(POMODORO_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
if(POMODORO_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # MSVC
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(pomodoro_tests tests/pomodoro_test.cpp)
  target_link_libraries(pomodoro_tests PRIVATE pomodoro_core GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(pomodoro_tests)
endif()
