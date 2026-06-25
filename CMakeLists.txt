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
