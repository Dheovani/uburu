find_package(Catch2 3 CONFIG REQUIRED)

add_executable(uburu_core_tests
  unit/bounded-queue-tests.cpp
  unit/direct-search-engine-tests.cpp
  unit/git-ignore-rules-tests.cpp
  unit/path-normalization-tests.cpp
  unit/polling-file-watcher-tests.cpp
  unit/recursive-file-scanner-tests.cpp
  unit/regex-matcher-tests.cpp
  unit/search-query-validation-tests.cpp
  unit/text-file-reader-tests.cpp
  unit/text-matcher-tests.cpp
  unit/worker-pool-tests.cpp
)

target_link_libraries(uburu_core_tests PRIVATE Uburu::Core Uburu::ProjectOptions Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(uburu_core_tests)
