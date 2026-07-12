add_library(uburu_cli_lib STATIC
  src/cli-options.cpp
  src/cli-output.cpp
)
add_library(Uburu::Cli ALIAS uburu_cli_lib)

target_include_directories(uburu_cli_lib PUBLIC
  ${PROJECT_SOURCE_DIR}/apps/cli/src
  ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(uburu_cli_lib PUBLIC Uburu::App Uburu::Core Uburu::ProjectOptions)

add_executable(uburu src/main.cpp)
target_link_libraries(uburu PRIVATE Uburu::Cli Uburu::ProjectOptions)

if(WIN32)
  add_custom_command(TARGET uburu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:uburu> $<TARGET_RUNTIME_DLLS:uburu>
    COMMAND_EXPAND_LISTS
  )
endif()
