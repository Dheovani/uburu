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

add_subdirectory(src)

if(UBURU_BUILD_DESKTOP)
  add_subdirectory(apps/desktop)
endif()

if(UBURU_BUILD_TESTS)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
