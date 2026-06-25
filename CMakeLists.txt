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

add_subdirectory(src)

if(UBURU_BUILD_DESKTOP)
  add_subdirectory(apps/desktop)
endif()

if(UBURU_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
