find_package(SQLite3 REQUIRED)
find_package(benchmark CONFIG REQUIRED)

add_library(uburu_benchmark_support
  support/benchmark-dataset.cpp
  support/benchmark-output.cpp
  support/index-benchmark-runner.cpp
  support/search-benchmark-runner.cpp
)

add_executable(uburu-storage-fts5-benchmark
  storage-fts5-benchmark.cpp
)

add_executable(uburu-content-hash-benchmark
  content-hash-benchmark.cpp
)

add_executable(uburu-search-service-benchmark
  search-service-benchmark.cpp
)

target_include_directories(uburu_benchmark_support
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/support
)

target_link_libraries(uburu_benchmark_support
  PUBLIC
    Uburu::App
    Uburu::Core
    benchmark::benchmark
  PRIVATE
    Uburu::ProjectOptions
)

target_link_libraries(uburu-storage-fts5-benchmark
  PRIVATE
    Uburu::ProjectOptions
)

target_link_libraries(uburu-content-hash-benchmark
  PRIVATE
    Uburu::Core
    Uburu::ProjectOptions
)

target_link_libraries(uburu-search-service-benchmark
  PRIVATE
    Uburu::ProjectOptions
    uburu_benchmark_support
    benchmark::benchmark_main
)

if(TARGET SQLite3::SQLite3)
  target_link_libraries(uburu-storage-fts5-benchmark PRIVATE SQLite3::SQLite3)
else()
  target_link_libraries(uburu-storage-fts5-benchmark PRIVATE SQLite::SQLite3)
endif()
