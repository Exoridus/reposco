include(ExternalProject)
find_package(Git REQUIRED)
file(READ "${PROJECT_SOURCE_DIR}/third_party/deps.lock.json" dependencyLock)
string(
  JSON
  opensslRevision
  GET
  "${dependencyLock}"
  sources
  4
  revision)
execute_process(
  COMMAND "${GIT_EXECUTABLE}" -C "${PROJECT_SOURCE_DIR}/third_party/openssl" rev-parse HEAD
  OUTPUT_VARIABLE opensslHead
  OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT opensslHead STREQUAL opensslRevision)
  message(FATAL_ERROR "OpenSSL checkout does not match the dependency lock")
endif()
set(REPOSCO_DEPS_ROOT "${PROJECT_BINARY_DIR}/deps")
set(opensslSource "${PROJECT_SOURCE_DIR}/third_party/openssl")
set(opensslInstall "${REPOSCO_DEPS_ROOT}/openssl-install")
find_program(
  REPOSCO_PERL
  NAMES perl.exe
  PATHS "${PROJECT_SOURCE_DIR}/build/tools/perl/perl/bin"
  NO_DEFAULT_PATH REQUIRED)
find_program(REPOSCO_NMAKE NAMES nmake REQUIRED)
file(MAKE_DIRECTORY "${opensslInstall}/include")
configure_file("${PROJECT_SOURCE_DIR}/third_party/deps.lock.json"
               "${REPOSCO_DEPS_ROOT}/openssl-revision.json" COPYONLY)
ExternalProject_Add(
  reposco_openssl_build
  SOURCE_DIR "${opensslSource}"
  BINARY_DIR "${REPOSCO_DEPS_ROOT}/openssl-build"
  DOWNLOAD_COMMAND ""
  UPDATE_COMMAND ""
  CONFIGURE_COMMAND "${REPOSCO_PERL}" "${opensslSource}/Configure" VC-WIN64A no-shared no-tests
                    no-apps no-module no-asm "--prefix=${opensslInstall}" "--libdir=lib"
  BUILD_COMMAND "${REPOSCO_NMAKE}" /nologo build_libs
  INSTALL_COMMAND "${REPOSCO_NMAKE}" /nologo install_dev
  BUILD_BYPRODUCTS "${opensslInstall}/lib/libcrypto.lib"
  LOG_CONFIGURE ON
  LOG_BUILD ON
  LOG_INSTALL ON)
ExternalProject_Add_StepDependencies(reposco_openssl_build configure
                                     "${REPOSCO_DEPS_ROOT}/openssl-revision.json")
add_library(reposco_crypto STATIC IMPORTED GLOBAL)
set_target_properties(
  reposco_crypto
  PROPERTIES IMPORTED_LOCATION "${opensslInstall}/lib/libcrypto.lib"
             INTERFACE_INCLUDE_DIRECTORIES "${opensslInstall}/include"
             INTERFACE_LINK_LIBRARIES "ws2_32;crypt32;advapi32;user32")
add_dependencies(reposco_crypto reposco_openssl_build)

set(sqlcipherSource "${PROJECT_SOURCE_DIR}/third_party/sqlcipher")
set(sqlcipherGenerated "${REPOSCO_DEPS_ROOT}/sqlcipher-generated")
file(MAKE_DIRECTORY "${sqlcipherGenerated}")
file(TO_NATIVE_PATH "${sqlcipherSource}" sqlcipherNative)
add_custom_command(
  OUTPUT "${sqlcipherGenerated}/sqlite3.c" "${sqlcipherGenerated}/sqlite3.h"
  COMMAND "${REPOSCO_NMAKE}" /nologo /f "${sqlcipherNative}/Makefile.msc" "TOP=${sqlcipherNative}"
          sqlite3.c
  WORKING_DIRECTORY "${sqlcipherGenerated}"
  COMMENT "Generating SQLCipher source with upstream nmake"
  DEPENDS "${sqlcipherSource}/manifest.uuid" "${sqlcipherSource}/Makefile.msc"
  VERBATIM)
add_library(reposco_sqlcipher STATIC "${sqlcipherGenerated}/sqlite3.c")
target_include_directories(reposco_sqlcipher SYSTEM PUBLIC "${sqlcipherGenerated}")
target_compile_definitions(
  reposco_sqlcipher
  PUBLIC SQLITE_HAS_CODEC
  PRIVATE SQLITE_TEMP_STORE=3
          SQLITE_THREADSAFE=1
          SQLCIPHER_CRYPTO_OPENSSL
          SQLITE_EXTRA_INIT=sqlcipher_extra_init
          SQLITE_EXTRA_SHUTDOWN=sqlcipher_extra_shutdown
          SQLITE_OMIT_LOAD_EXTENSION
          SQLITE_ENABLE_FTS5)
target_link_libraries(reposco_sqlcipher PUBLIC reposco_crypto)
add_dependencies(reposco_sqlcipher reposco_openssl_build)

set(QWINDOWKIT_BUILD_STATIC
    ON
    CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_QUICK
    ON
    CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_WIDGETS
    OFF
    CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_EXAMPLES
    OFF
    CACHE BOOL "" FORCE)
set(QWINDOWKIT_BUILD_DOCUMENTATIONS
    OFF
    CACHE BOOL "" FORCE)
set(QWINDOWKIT_INSTALL
    OFF
    CACHE BOOL "" FORCE)
add_subdirectory(third_party/qwindowkit EXCLUDE_FROM_ALL)
