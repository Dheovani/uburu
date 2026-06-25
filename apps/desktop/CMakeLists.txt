find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Concurrent LinguistTools)

qt_standard_project_setup(REQUIRES 6.5 I18N_TRANSLATED_LANGUAGES pt_BR en_US)

qt_add_executable(uburu_desktop
  src/main.cpp
  src/search-controller.cpp
  src/search-controller.hpp
)

qt_add_resources(uburu_desktop "uburu_qml"
  PREFIX "/"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}"
  FILES
    qml/main.qml
    qml/components/PrimaryButton.qml
    qml/components/SecondaryButton.qml
    qml/theme/Theme.qml
    qml/theme/qmldir
)

qt_add_translations(uburu_desktop
  TS_FILES i18n/uburu-en-us.ts i18n/uburu-pt-br.ts
  RESOURCE_PREFIX /i18n
)

target_link_libraries(uburu_desktop PRIVATE
  Uburu::App Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent
)

set_target_properties(uburu_desktop PROPERTIES
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE TRUE
)
