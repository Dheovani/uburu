add_library(uburu_core STATIC
  filesystem/recursive-file-scanner.cpp
  search/direct-search-engine.cpp
  text/text-matcher.cpp
)

add_library(Uburu::Core ALIAS uburu_core)

target_include_directories(uburu_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_features(uburu_core PUBLIC cxx_std_23)

if(MSVC)
  target_compile_options(uburu_core PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(uburu_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
  if(TARGET SQLite3::SQLite3)
    target_link_libraries(uburu_core PRIVATE SQLite3::SQLite3)
  else()
    target_link_libraries(uburu_core PRIVATE SQLite::SQLite3)
  endif()
  target_compile_definitions(uburu_core PUBLIC UBURU_HAS_SQLITE=1)
endif()

if(UBURU_ENABLE_PCRE2)
  find_package(PCRE2 CONFIG QUIET)
  if(PCRE2_FOUND)
    target_link_libraries(uburu_core PRIVATE PCRE2::8BIT)
    target_compile_definitions(uburu_core PUBLIC UBURU_HAS_PCRE2=1)
  endif()
endif()

if(UBURU_ENABLE_LIBGIT2)
  find_package(libgit2 CONFIG QUIET)
  if(libgit2_FOUND)
    if(TARGET libgit2::libgit2package)
      target_link_libraries(uburu_core PRIVATE libgit2::libgit2package)
    elseif(TARGET libgit2::libgit2)
      target_link_libraries(uburu_core PRIVATE libgit2::libgit2)
    endif()
    target_compile_definitions(uburu_core PUBLIC UBURU_HAS_LIBGIT2=1)
  endif()
endif()
