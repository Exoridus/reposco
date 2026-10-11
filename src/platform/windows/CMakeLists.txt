add_library(
  reposco_platform_windows STATIC Dpapi.cpp GlobalHotkey.cpp KeyVault.cpp
                                  NativeClipboardMonitor.cpp ../interfaces/ClipboardMonitor.h)
target_include_directories(reposco_platform_windows PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  reposco_platform_windows
  PUBLIC Qt6::Core reposco_content
  PRIVATE crypt32 user32 bcrypt ole32 reposco_core)
reposco_owned_target(reposco_platform_windows)
