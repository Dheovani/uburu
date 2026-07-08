find_package(Catch2 3 CONFIG REQUIRED)

add_executable(uburu_core_tests
  integration/libgit2-git-service-integration-tests.cpp
  regression/search-regression-tests.cpp
  unit/adaptive-result-batcher-tests.cpp
  unit/bounded-queue-tests.cpp
  unit/content-hash-tests.cpp
  unit/diagnostics-tests.cpp
  unit/document-extractor-tests.cpp
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
  unit/rich-format-safety-tests.cpp
  unit/search-query-validation-tests.cpp
  unit/search-dto-tests.cpp
  unit/search-result-merge-tests.cpp
  unit/search-scope-tests.cpp
  unit/search-service-tests.cpp
  unit/settings-service-tests.cpp
  unit/sqlite-storage-service-tests.cpp
  unit/text-file-reader-tests.cpp
  unit/text-matcher-tests.cpp
  unit/worker-pool-tests.cpp
)

target_link_libraries(uburu_core_tests PRIVATE Uburu::App Uburu::Core Uburu::ProjectOptions Catch2::Catch2WithMain)
target_include_directories(uburu_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

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
set_property(TARGET uburu_core_tests
  PROPERTY CROSSCOMPILING_EMULATOR
    ${CMAKE_COMMAND} -E env GIT_CONFIG_COUNT=0 GIT_CONFIG_KEY_0= GIT_CONFIG_VALUE_0=
)
catch_discover_tests(uburu_core_tests)

if(UBURU_BUILD_DESKTOP)
  find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Concurrent)
  qt6_wrap_cpp(UBURU_DESKTOP_TEST_MOC_SOURCES ../apps/desktop/src/search-controller.hpp)

  qt_add_executable(uburu_desktop_tests
    desktop/search-controller-tests.cpp
    ../apps/desktop/src/search-controller.cpp
    ../apps/desktop/src/search-controller.hpp
    ${UBURU_DESKTOP_TEST_MOC_SOURCES}
  )

  set_target_properties(uburu_desktop_tests PROPERTIES AUTOMOC ON)

  target_include_directories(uburu_desktop_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/apps/desktop/src
  )

  target_link_libraries(uburu_desktop_tests PRIVATE
    Uburu::App
    Uburu::Core
    Uburu::ProjectOptions
    Catch2::Catch2WithMain
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
  )

  if(WIN32)
    add_custom_command(TARGET uburu_desktop_tests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:uburu_desktop_tests> $<TARGET_RUNTIME_DLLS:uburu_desktop_tests>
      COMMAND_EXPAND_LISTS
    )
  endif()

  catch_discover_tests(uburu_desktop_tests)
endif()
