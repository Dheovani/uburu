add_library(uburu_app STATIC
  dto/search-dto.cpp
  services/adaptive-result-batcher.cpp
  services/indexing-service.cpp
  services/search-service.cpp
)
add_library(Uburu::App ALIAS uburu_app)
target_include_directories(uburu_app PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(uburu_app PUBLIC Uburu::Core)
