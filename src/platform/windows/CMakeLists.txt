add_library(reposco_platform_windows STATIC Dpapi.cpp GlobalHotkey.cpp)
target_include_directories(reposco_platform_windows PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  reposco_platform_windows
  PUBLIC Qt6::Core
  PRIVATE crypt32 user32)
reposco_owned_target(reposco_platform_windows)
