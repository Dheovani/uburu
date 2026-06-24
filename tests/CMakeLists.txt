find_package(Catch2 3 CONFIG REQUIRED)

add_executable(uburu_core_tests
  unit/direct-search-engine-tests.cpp
  unit/recursive-file-scanner-tests.cpp
  unit/regex-matcher-tests.cpp
  unit/search-query-validation-tests.cpp
  unit/text-matcher-tests.cpp
)

target_link_libraries(uburu_core_tests PRIVATE Uburu::Core Catch2::Catch2WithMain)
target_compile_features(uburu_core_tests PRIVATE cxx_std_23)

include(Catch)
catch_discover_tests(uburu_core_tests)
