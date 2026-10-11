add_library(reposco_history STATIC HistoryWorker.cpp HistoryWorker.h)
target_include_directories(reposco_history PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(reposco_history PUBLIC reposco_storage reposco_platform_windows Qt6::Core)
reposco_owned_target(reposco_history)

qt_add_library(
  reposco_ui
  STATIC
  ../ui/DesktopSession.cpp
  ../ui/DesktopSession.h
  ../ui/HistoryModel.cpp
  ../ui/HistoryModel.h
  ../ui/HistoryController.cpp
  ../ui/HistoryController.h)
target_include_directories(
  reposco_ui
  PUBLIC "${PROJECT_SOURCE_DIR}/src"
  PRIVATE "${PROJECT_SOURCE_DIR}/src/ui")
target_link_libraries(
  reposco_ui
  PUBLIC Qt6::Quick Qt6::Widgets Qt6::Svg reposco_history
  PRIVATE QWKQuick)
reposco_owned_target(reposco_ui)
set_source_files_properties(../../qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_property(SOURCE ../../qml/SettingsWindow.qml PROPERTY QT_RESOURCE_ALIAS SettingsWindow.qml)
set(paletteComponents
    ../../qml/components/StyleTokens.qml
    ../../qml/components/GlassSurface.qml
    ../../qml/components/ActionButton.qml
    ../../qml/components/PolicySwitch.qml
    ../../qml/components/PaletteSearch.qml
    ../../qml/components/PreviewPane.qml
    ../../qml/components/HistorySlot.qml
    ../../qml/components/EmptyState.qml)
foreach(component IN LISTS paletteComponents)
  get_filename_component(componentName "${component}" NAME)
  set_property(SOURCE ${component} PROPERTY QT_RESOURCE_ALIAS "components/${componentName}")
endforeach()
set(softGlass ../../qml/components/SoftGlass.qml)
set_source_files_properties(${softGlass} PROPERTIES QT_RESOURCE_ALIAS components/SoftGlass.qml)
qt_add_qml_module(
  reposco_ui
  URI
  Reposco
  VERSION
  1.0
  RESOURCE_PREFIX
  /qt/qml
  OUTPUT_DIRECTORY
  "${PROJECT_BINARY_DIR}/qml/Reposco"
  QML_FILES
  ../../qml/Main.qml
  ../../qml/SettingsWindow.qml
  ${paletteComponents}
  ../../qml/components/SoftGlass.qml)
if(MSVC)
  # Qt 6.12's generated QML casts trigger C4702 in Qt headers with newer MSVC. Owned C++ retains
  # /WX; this exception covers only the affected generated QML caches.
  set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/.rcc/qmlcache/reposco_ui___qml_Main_qml.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/.rcc/qmlcache/reposco_ui___qml_SettingsWindow_qml.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/.rcc/qmlcache/reposco_ui___qml_components_HistorySlot_qml.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/.rcc/qmlcache/reposco_ui___qml_components_GlassSurface_qml.cpp"
    PROPERTIES COMPILE_OPTIONS /wd4702)
endif()
configure_file(reposco.rc.in "${CMAKE_CURRENT_BINARY_DIR}/reposco.rc" @ONLY)
qt_add_executable(reposco WIN32 Main.cpp "${CMAKE_CURRENT_BINARY_DIR}/reposco.rc")
target_link_libraries(reposco PRIVATE reposco_ui reposco_uiplugin)
target_compile_definitions(reposco PRIVATE REPOSCO_VERSION="${REPOSCO_VERSION}")
reposco_owned_target(reposco)
qt_add_resources(
  reposco_ui
  branding
  PREFIX
  /assets
  BASE
  "${PROJECT_SOURCE_DIR}/assets"
  FILES
  ../../assets/branding/reposco.ico
  ../../assets/branding/reposco-icon-primary.png
  ../../assets/branding/reposco-wordmark-primary.png
  ../../assets/branding/reposco-wordmark-dark.png)
if(BUILD_TESTING)
  qt_add_executable(desktop_tests ../../tests/integration/DesktopTests.cpp)
  target_link_libraries(desktop_tests PRIVATE reposco_ui reposco_uiplugin Qt6::Test)
  reposco_owned_target(desktop_tests)
  set_target_properties(desktop_tests PROPERTIES WIN32_EXECUTABLE FALSE)
  add_test(NAME integration.desktop COMMAND desktop_tests -o -,txt)
  set_tests_properties(integration.desktop PROPERTIES LABELS "windows;desktop" TIMEOUT 30)
endif()
