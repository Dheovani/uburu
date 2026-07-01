find_package(Catch2 3 CONFIG REQUIRED)

add_executable(uburu_core_tests
  unit/bounded-queue-tests.cpp
  unit/content-hash-tests.cpp
  unit/diagnostics-tests.cpp
  unit/direct-search-engine-tests.cpp
  unit/git-reconciliation-tests.cpp
  unit/git-ignore-rules-tests.cpp
  unit/indexing-service-tests.cpp
  unit/index-document-format-tests.cpp
  unit/index-overlay-tests.cpp
  unit/libgit2-git-service-tests.cpp
  unit/path-normalization-tests.cpp
  unit/persistent-index-service-tests.cpp
  unit/polling-file-watcher-tests.cpp
  unit/recursive-file-scanner-tests.cpp
  unit/regex-matcher-tests.cpp
  unit/search-query-validation-tests.cpp
  unit/search-result-merge-tests.cpp
  unit/search-service-tests.cpp
  unit/sqlite-storage-service-tests.cpp
  unit/text-file-reader-tests.cpp
  unit/text-matcher-tests.cpp
  unit/worker-pool-tests.cpp
)

target_link_libraries(uburu_core_tests PRIVATE Uburu::App Uburu::Core Uburu::ProjectOptions Catch2::Catch2WithMain)

if(WIN32)
  add_custom_command(TARGET uburu_core_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:uburu_core_tests> $<TARGET_RUNTIME_DLLS:uburu_core_tests>
    COMMAND_EXPAND_LISTS
  )

  if(MINGW)
    get_filename_component(uburu_mingw_runtime_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(uburu_mingw_runtime_dlls)

    foreach(uburu_mingw_runtime_dll IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
      if(EXISTS "${uburu_mingw_runtime_directory}/${uburu_mingw_runtime_dll}")
        list(APPEND uburu_mingw_runtime_dlls "${uburu_mingw_runtime_directory}/${uburu_mingw_runtime_dll}")
      endif()
    endforeach()

    if(uburu_mingw_runtime_dlls)
      add_custom_command(TARGET uburu_core_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${uburu_mingw_runtime_dlls} $<TARGET_FILE_DIR:uburu_core_tests>
        COMMAND_EXPAND_LISTS
      )
    endif()
  endif()
endif()

include(Catch)
catch_discover_tests(uburu_core_tests)
