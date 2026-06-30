find_package(SQLite3 REQUIRED)

add_executable(uburu-storage-fts5-benchmark
  storage-fts5-benchmark.cpp
)

add_executable(uburu-content-hash-benchmark
  content-hash-benchmark.cpp
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

if(TARGET SQLite3::SQLite3)
  target_link_libraries(uburu-storage-fts5-benchmark PRIVATE SQLite3::SQLite3)
else()
  target_link_libraries(uburu-storage-fts5-benchmark PRIVATE SQLite::SQLite3)
endif()
