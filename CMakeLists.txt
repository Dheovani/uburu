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
option(UBURU_WARNINGS_AS_ERRORS "Treat compiler warnings as build errors for project targets" OFF)
option(UBURU_BUILD_BENCHMARKS "Build developer benchmarks" OFF)

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

    add_custom_target(format
      COMMAND "${UBURU_CLANG_FORMAT_EXECUTABLE}" -i ${UBURU_FORMAT_SOURCES}
      COMMENT "Formatting project C++ sources"
      VERBATIM
    )

    add_custom_target(format-check
      COMMAND "${UBURU_CLANG_FORMAT_EXECUTABLE}" --dry-run --Werror ${UBURU_FORMAT_SOURCES}
      COMMENT "Checking project C++ source formatting"
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
      "${PROJECT_SOURCE_DIR}/benchmarks/*.cpp"
      "${PROJECT_SOURCE_DIR}/src/*.cpp"
      "${PROJECT_SOURCE_DIR}/tests/*.cpp"
    )

    if(NOT UBURU_BUILD_DESKTOP)
      list(FILTER UBURU_TIDY_SOURCES EXCLUDE REGEX "/src/app/")
    endif()

    if(UBURU_BUILD_DESKTOP)
      file(GLOB_RECURSE UBURU_TIDY_DESKTOP_SOURCES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/apps/*.cpp"
      )
      list(APPEND UBURU_TIDY_SOURCES ${UBURU_TIDY_DESKTOP_SOURCES})
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

if(UBURU_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()
