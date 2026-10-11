cmake_minimum_required(VERSION 3.27)
project(
  Reposco
  VERSION 0.1.0
  LANGUAGES C CXX)

set(REPOSCO_VERSION "${PROJECT_VERSION}-dev")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(REPOSCO_BUILD_APP "Build the Windows desktop integration proof" ${WIN32})
option(REPOSCO_USE_SCCACHE "Use the optional sccache compiler launcher" OFF)
option(REPOSCO_SANITIZERS "Enable ASan/UBSan for portable targets" OFF)
option(REPOSCO_ISOLATED_CLIPBOARD_TESTS
       "Enable private window-station tests requiring Windows privileges" OFF)
if(REPOSCO_SANITIZERS
   AND (MSVC
        OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"
        OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$"))
  message(FATAL_ERROR "ASan/UBSan presets require GCC or Clang with the GNU frontend")
endif()
include(CTest)
include(cmake/Warnings.cmake)

add_library(reposco_core STATIC src/core/ContentPolicy.cpp src/core/CapturePolicy.cpp)
target_include_directories(reposco_core PUBLIC src/core)
reposco_owned_target(reposco_core)

if(BUILD_TESTING)
  add_executable(core_tests tests/unit/CoreTests.cpp)
  target_link_libraries(core_tests PRIVATE reposco_core)
  reposco_owned_target(core_tests)
  add_test(NAME core.policy COMMAND core_tests)
  set_tests_properties(core.policy PROPERTIES LABELS "unit;portable" TIMEOUT 15)
endif()

if(REPOSCO_BUILD_APP)
  if(NOT WIN32)
    message(FATAL_ERROR "Only the portable core is supported outside Windows")
  endif()
  if(NOT Qt6_DIR
     AND NOT CMAKE_PREFIX_PATH
     AND EXISTS "C:/Qt/6.12.0/msvc2022_64")
    list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.12.0/msvc2022_64")
  endif()
  set(CMAKE_INSTALL_BINDIR ".")
  find_package(
    Qt6 6.12.0 EXACT REQUIRED
    COMPONENTS Core
               Gui
               Qml
               Quick
               QuickControls2
               Widgets
               Svg
               Test
               QuickTest)
  # QWindowKit's private Qt targets must be visible to Qt's root-level deployment traversal.
  set(QT_NO_PRIVATE_MODULE_WARNING ON)
  find_package(Qt6 6.12.0 EXACT REQUIRED COMPONENTS CorePrivate GuiPrivate QuickPrivate)
  qt_standard_project_setup(REQUIRES 6.12)
  include(cmake/Dependencies.cmake)
  add_library(reposco_text STATIC src/core/TextContent.cpp)
  target_include_directories(reposco_text PUBLIC "${PROJECT_SOURCE_DIR}/src")
  target_link_libraries(
    reposco_text
    PUBLIC Qt6::Core
    PRIVATE reposco_core)
  reposco_owned_target(reposco_text)
  add_subdirectory(src/platform/windows)
  add_subdirectory(src/storage)
  add_subdirectory(src/app)
  if(BUILD_TESTING)
    add_subdirectory(tests/integration)
    add_subdirectory(tests/qml)
  endif()
  include(cmake/Packaging.cmake)
endif()
