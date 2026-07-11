cmake_minimum_required(VERSION 3.25)

project(Uburu VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(UBURU_BUILD_DESKTOP "Build the Qt desktop application" ON)
option(UBURU_BUILD_TESTS "Build automated tests" ON)
option(UBURU_ENABLE_PCRE2 "Enable the future PCRE2 regex backend" ON)
option(UBURU_ENABLE_LIBGIT2 "Enable the future libgit2 backend" ON)
option(UBURU_ENABLE_FORMAT_TARGET "Add the optional clang-format target" ON)
option(UBURU_ENABLE_TIDY_TARGET "Add the optional clang-tidy target" ON)
option(UBURU_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer where supported" OFF)
option(UBURU_ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer where supported" OFF)
option(UBURU_ENABLE_COVERAGE "Enable coverage instrumentation and report targets where supported" OFF)
option(UBURU_ENABLE_FUZZING "Enable libFuzzer instrumentation where supported" OFF)
option(UBURU_WARNINGS_AS_ERRORS "Treat compiler warnings as build errors for project targets" OFF)
option(UBURU_BUILD_BENCHMARKS "Build developer benchmarks" OFF)
option(UBURU_BUILD_FUZZERS "Build fuzz targets" OFF)
set(UBURU_COVERAGE_CORE_LINE_THRESHOLD "30" CACHE STRING "Minimum line coverage percentage for src/core")
set(UBURU_COVERAGE_CORE_BRANCH_THRESHOLD "15" CACHE STRING "Minimum branch coverage percentage for src/core")
set(UBURU_COVERAGE_APP_LINE_THRESHOLD "30" CACHE STRING "Minimum line coverage percentage for src/app")
set(UBURU_COVERAGE_APP_BRANCH_THRESHOLD "15" CACHE STRING "Minimum branch coverage percentage for src/app")

add_library(uburu_project_options INTERFACE)
add_library(Uburu::ProjectOptions ALIAS uburu_project_options)

target_compile_features(uburu_project_options INTERFACE cxx_std_23)

if(MSVC)
  target_compile_options(uburu_project_options INTERFACE /W4 /permissive- /utf-8)

  if(UBURU_WARNINGS_AS_ERRORS)
    target_compile_options(uburu_project_options INTERFACE /WX)
  endif()
else()
  target_compile_options(uburu_project_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion)

  if(UBURU_WARNINGS_AS_ERRORS)
    target_compile_options(uburu_project_options INTERFACE -Werror)
  endif()
endif()

if(UBURU_ENABLE_SANITIZERS AND UBURU_ENABLE_THREAD_SANITIZER)
  message(FATAL_ERROR "UBURU_ENABLE_SANITIZERS and UBURU_ENABLE_THREAD_SANITIZER cannot be enabled together")
endif()

if(UBURU_ENABLE_COVERAGE AND (UBURU_ENABLE_SANITIZERS OR UBURU_ENABLE_THREAD_SANITIZER))
  message(FATAL_ERROR "UBURU_ENABLE_COVERAGE cannot be enabled together with sanitizer instrumentation")
endif()

if(UBURU_ENABLE_FUZZING AND (UBURU_ENABLE_COVERAGE OR UBURU_ENABLE_SANITIZERS OR UBURU_ENABLE_THREAD_SANITIZER))
  message(FATAL_ERROR "UBURU_ENABLE_FUZZING cannot be enabled together with coverage or sanitizer presets")
endif()

if(UBURU_BUILD_FUZZERS AND NOT UBURU_ENABLE_FUZZING)
  message(FATAL_ERROR "UBURU_BUILD_FUZZERS requires UBURU_ENABLE_FUZZING")
endif()

if(UBURU_ENABLE_SANITIZERS)
  if(MSVC)
    message(WARNING "UBURU_ENABLE_SANITIZERS is not configured for MSVC yet")
  else()
    target_compile_options(uburu_project_options INTERFACE
      -fsanitize=address,undefined
      -fno-omit-frame-pointer
    )
    target_link_options(uburu_project_options INTERFACE -fsanitize=address,undefined)
  endif()
endif()

if(UBURU_ENABLE_THREAD_SANITIZER)
  if(MSVC)
    message(WARNING "UBURU_ENABLE_THREAD_SANITIZER is not configured for MSVC")
  else()
    target_compile_options(uburu_project_options INTERFACE
      -fsanitize=thread
      -fno-omit-frame-pointer
    )
    target_link_options(uburu_project_options INTERFACE -fsanitize=thread)
  endif()
endif()

if(UBURU_ENABLE_COVERAGE)
  if(MSVC)
    message(FATAL_ERROR "UBURU_ENABLE_COVERAGE is not supported with MSVC")
  else()
    target_compile_options(uburu_project_options INTERFACE
      -O0
      -g
      --coverage
    )
    target_link_options(uburu_project_options INTERFACE --coverage)
  endif()
endif()

if(UBURU_ENABLE_FUZZING)
  if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "UBURU_ENABLE_FUZZING requires Clang/libFuzzer")
  endif()

  target_compile_options(uburu_project_options INTERFACE
    -fsanitize=fuzzer-no-link,address,undefined
    -fno-omit-frame-pointer
  )
  target_link_options(uburu_project_options INTERFACE -fsanitize=address,undefined)
endif()

if(UBURU_ENABLE_FORMAT_TARGET)
  find_program(UBURU_CLANG_FORMAT_EXECUTABLE
    NAMES clang-format clang-format.exe
    HINTS
      "$ENV{ProgramFiles}/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/bin"
      "$ENV{ProgramFiles}/Microsoft Visual Studio/18/Professional/VC/Tools/Llvm/bin"
      "$ENV{ProgramFiles}/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/bin"
      "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin"
      "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/Professional/VC/Tools/Llvm/bin"
      "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/bin"
  )

  if(UBURU_CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE UBURU_FORMAT_SOURCES CONFIGURE_DEPENDS
      "${PROJECT_SOURCE_DIR}/apps/*.cpp"
      "${PROJECT_SOURCE_DIR}/apps/*.h"
      "${PROJECT_SOURCE_DIR}/apps/*.hpp"
      "${PROJECT_SOURCE_DIR}/benchmarks/*.cpp"
      "${PROJECT_SOURCE_DIR}/benchmarks/*.h"
      "${PROJECT_SOURCE_DIR}/benchmarks/*.hpp"
      "${PROJECT_SOURCE_DIR}/src/*.cpp"
      "${PROJECT_SOURCE_DIR}/src/*.h"
      "${PROJECT_SOURCE_DIR}/src/*.hpp"
      "${PROJECT_SOURCE_DIR}/tests/*.cpp"
      "${PROJECT_SOURCE_DIR}/tests/*.h"
      "${PROJECT_SOURCE_DIR}/tests/*.hpp"
    )

    string(REPLACE ";" "\n" UBURU_FORMAT_SOURCE_LIST_CONTENT "${UBURU_FORMAT_SOURCES}")
    set(UBURU_FORMAT_SOURCE_LIST "${CMAKE_BINARY_DIR}/uburu-format-sources.txt")
    file(WRITE "${UBURU_FORMAT_SOURCE_LIST}" "${UBURU_FORMAT_SOURCE_LIST_CONTENT}\n")

    add_custom_target(format
      COMMAND "${CMAKE_COMMAND}" -E echo
        "Global clang-format is disabled; use targeted formatting that preserves Uburu's project style."
      COMMENT "Skipping global automatic formatting"
      VERBATIM
    )

    add_custom_target(format-check
      COMMAND "${CMAKE_COMMAND}" "-DUBURU_FORMAT_SOURCE_LIST=${UBURU_FORMAT_SOURCE_LIST}"
        -P "${PROJECT_SOURCE_DIR}/cmake/check-cpp-style.cmake"
      COMMENT "Checking project C++ source style baseline"
      VERBATIM
    )
  else()
    message(STATUS "clang-format not found; the optional format targets are unavailable")
  endif()
endif()

if(UBURU_ENABLE_TIDY_TARGET)
  find_program(UBURU_CLANG_TIDY_EXECUTABLE
    NAMES clang-tidy clang-tidy.exe
    HINTS
      "$ENV{ProgramFiles}/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/bin"
      "$ENV{ProgramFiles}/Microsoft Visual Studio/18/Professional/VC/Tools/Llvm/bin"
      "$ENV{ProgramFiles}/Microsoft Visual Studio/18/Enterprise/VC/Tools/Llvm/bin"
      "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/bin"
      "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/Professional/VC/Tools/Llvm/bin"
      "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/bin"
  )

  if(UBURU_CLANG_TIDY_EXECUTABLE)
    file(GLOB_RECURSE UBURU_TIDY_SOURCES CONFIGURE_DEPENDS
      "${PROJECT_SOURCE_DIR}/src/*.cpp"
      "${PROJECT_SOURCE_DIR}/tests/*.cpp"
    )

    if(NOT UBURU_BUILD_TESTS)
      list(FILTER UBURU_TIDY_SOURCES EXCLUDE REGEX "/tests/")
    endif()

    if(NOT UBURU_BUILD_FUZZERS)
      list(FILTER UBURU_TIDY_SOURCES EXCLUDE REGEX "/tests/fuzzers/")
    endif()

    if(NOT WIN32)
      list(FILTER UBURU_TIDY_SOURCES EXCLUDE REGEX "/filesystem/windows-file-watcher\\.cpp$")
    endif()

    if(NOT APPLE)
      list(FILTER UBURU_TIDY_SOURCES EXCLUDE REGEX "/filesystem/macos-file-watcher\\.cpp$")
    endif()

    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
      list(FILTER UBURU_TIDY_SOURCES EXCLUDE REGEX "/filesystem/linux-file-watcher\\.cpp$")
    endif()

    if(UBURU_BUILD_BENCHMARKS)
      file(GLOB_RECURSE UBURU_TIDY_BENCHMARK_SOURCES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/benchmarks/*.cpp"
      )
      list(APPEND UBURU_TIDY_SOURCES ${UBURU_TIDY_BENCHMARK_SOURCES})
    endif()

    if(UBURU_BUILD_DESKTOP)
      file(GLOB_RECURSE UBURU_TIDY_DESKTOP_SOURCES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/apps/*.cpp"
      )
      list(APPEND UBURU_TIDY_SOURCES ${UBURU_TIDY_DESKTOP_SOURCES})
    else()
      list(FILTER UBURU_TIDY_SOURCES EXCLUDE REGEX "/tests/desktop/")
    endif()

    set(UBURU_TIDY_RELATIVE_SOURCES)
    foreach(UBURU_TIDY_SOURCE IN LISTS UBURU_TIDY_SOURCES)
      file(RELATIVE_PATH UBURU_TIDY_RELATIVE_SOURCE
        "${PROJECT_SOURCE_DIR}"
        "${UBURU_TIDY_SOURCE}"
      )
      list(APPEND UBURU_TIDY_RELATIVE_SOURCES "${UBURU_TIDY_RELATIVE_SOURCE}")
    endforeach()

    add_custom_target(tidy
      COMMAND "${UBURU_CLANG_TIDY_EXECUTABLE}" -p "${CMAKE_BINARY_DIR}" ${UBURU_TIDY_RELATIVE_SOURCES}
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      COMMENT "Running clang-tidy on project C++ sources"
      VERBATIM
    )
  else()
    message(STATUS "clang-tidy not found; the optional tidy target is unavailable")
  endif()
endif()

add_subdirectory(src)

if(UBURU_BUILD_DESKTOP)
  add_subdirectory(apps/desktop)
endif()

if(UBURU_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

if(UBURU_ENABLE_COVERAGE AND UBURU_BUILD_TESTS)
  find_program(UBURU_GCOVR_EXECUTABLE NAMES gcovr)

  if(UBURU_GCOVR_EXECUTABLE)
    set(UBURU_COVERAGE_DIRECTORY "${CMAKE_BINARY_DIR}/coverage")

    add_custom_target(coverage-core
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${UBURU_COVERAGE_DIRECTORY}"
      COMMAND "${UBURU_GCOVR_EXECUTABLE}"
        --root "${PROJECT_SOURCE_DIR}"
        --filter "${PROJECT_SOURCE_DIR}/src/core"
        --exclude "${PROJECT_SOURCE_DIR}/tests"
        --html-details "${UBURU_COVERAGE_DIRECTORY}/core.html"
        --xml "${UBURU_COVERAGE_DIRECTORY}/core.xml"
        --print-summary
        --fail-under-line "${UBURU_COVERAGE_CORE_LINE_THRESHOLD}"
        --fail-under-branch "${UBURU_COVERAGE_CORE_BRANCH_THRESHOLD}"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      COMMENT "Generating core coverage report"
      VERBATIM
    )

    add_custom_target(coverage-app
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${UBURU_COVERAGE_DIRECTORY}"
      COMMAND "${UBURU_GCOVR_EXECUTABLE}"
        --root "${PROJECT_SOURCE_DIR}"
        --filter "${PROJECT_SOURCE_DIR}/src/app"
        --exclude "${PROJECT_SOURCE_DIR}/tests"
        --html-details "${UBURU_COVERAGE_DIRECTORY}/app.html"
        --xml "${UBURU_COVERAGE_DIRECTORY}/app.xml"
        --print-summary
        --fail-under-line "${UBURU_COVERAGE_APP_LINE_THRESHOLD}"
        --fail-under-branch "${UBURU_COVERAGE_APP_BRANCH_THRESHOLD}"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      COMMENT "Generating application-service coverage report"
      VERBATIM
    )

    add_custom_target(coverage
      COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}" --output-on-failure
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${UBURU_COVERAGE_DIRECTORY}"
      COMMAND "${UBURU_GCOVR_EXECUTABLE}"
        --root "${PROJECT_SOURCE_DIR}"
        --filter "${PROJECT_SOURCE_DIR}/src/core"
        --exclude "${PROJECT_SOURCE_DIR}/tests"
        --html-details "${UBURU_COVERAGE_DIRECTORY}/core.html"
        --xml "${UBURU_COVERAGE_DIRECTORY}/core.xml"
        --print-summary
        --fail-under-line "${UBURU_COVERAGE_CORE_LINE_THRESHOLD}"
        --fail-under-branch "${UBURU_COVERAGE_CORE_BRANCH_THRESHOLD}"
      COMMAND "${UBURU_GCOVR_EXECUTABLE}"
        --root "${PROJECT_SOURCE_DIR}"
        --filter "${PROJECT_SOURCE_DIR}/src/app"
        --exclude "${PROJECT_SOURCE_DIR}/tests"
        --html-details "${UBURU_COVERAGE_DIRECTORY}/app.html"
        --xml "${UBURU_COVERAGE_DIRECTORY}/app.xml"
        --print-summary
        --fail-under-line "${UBURU_COVERAGE_APP_LINE_THRESHOLD}"
        --fail-under-branch "${UBURU_COVERAGE_APP_BRANCH_THRESHOLD}"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      COMMENT "Running tests and generating per-module coverage reports"
      VERBATIM
    )
  else()
    message(STATUS "gcovr not found; the optional coverage targets are unavailable")
  endif()
endif()

if(UBURU_BUILD_FUZZERS)
  add_subdirectory(tests/fuzzers)
endif()

if(UBURU_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
