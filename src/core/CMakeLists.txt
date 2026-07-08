add_library(uburu_core STATIC
  document/document-extractor.cpp
  document/plain-text-extractor.cpp
  diagnostics/diagnostic-report.cpp
  diagnostics/file-structured-logger.cpp
  diagnostics/search-tracing.cpp
  diagnostics/structured-logger.cpp
  diagnostics/structured-metrics-sink.cpp
  filesystem/git-ignore-rules.cpp
  filesystem/path-normalization.cpp
  filesystem/polling-file-watcher.cpp
  filesystem/recursive-file-scanner.cpp
  git/git-cli-git-service.cpp
  git/git-reconciliation.cpp
  git/libgit2-git-service.cpp
  index/content-hash.cpp
  index/index-document-format.cpp
  index/index-overlay.cpp
  index/persistent-index-service.cpp
  search/direct-search-engine.cpp
  search/search-errors.cpp
  search/search-query-validation.cpp
  search/search-result-merge.cpp
  search/search-scope.cpp
  storage/storage-paths.cpp
  storage/sqlite-storage-service.cpp
  text/rich-format-safety.cpp
  text/text-file-reader.cpp
  text/regex-matcher.cpp
  text/text-matcher.cpp
)

if(WIN32)
  target_sources(uburu_core PRIVATE filesystem/windows-file-watcher.cpp)
elseif(APPLE)
  target_sources(uburu_core PRIVATE filesystem/macos-file-watcher.cpp)
  target_link_libraries(uburu_core PRIVATE "-framework CoreServices")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(uburu_core PRIVATE filesystem/linux-file-watcher.cpp)
endif()

add_library(Uburu::Core ALIAS uburu_core)

target_include_directories(uburu_core PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(uburu_core PUBLIC Uburu::ProjectOptions)

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
