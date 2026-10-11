install(TARGETS reposco RUNTIME DESTINATION . COMPONENT Runtime)
qt_generate_deploy_qml_app_script(TARGET reposco OUTPUT_SCRIPT deployScript)
install(SCRIPT "${deployScript}" COMPONENT Runtime)
install(
  FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "${PROJECT_SOURCE_DIR}/PRIVACY.md"
  DESTINATION .
  COMPONENT Runtime)
install(
  FILES "${PROJECT_SOURCE_DIR}/third_party/deps.lock.json"
  DESTINATION licenses
  COMPONENT Runtime)
install(
  FILES "${PROJECT_SOURCE_DIR}/third_party/qwindowkit/LICENSE"
  DESTINATION licenses
  RENAME QWindowKit.txt
  COMPONENT Runtime)
install(
  FILES "${PROJECT_SOURCE_DIR}/third_party/sqlcipher/LICENSE.md"
  DESTINATION licenses
  RENAME SQLCipher.txt
  COMPONENT Runtime)
install(
  FILES "${PROJECT_SOURCE_DIR}/third_party/openssl/LICENSE.txt"
  DESTINATION licenses
  RENAME OpenSSL.txt
  COMPONENT Runtime)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME Reposco)
set(CPACK_PACKAGE_VERSION "${REPOSCO_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "Reposco-${REPOSCO_VERSION}-windows-x64")
set(CPACK_COMPONENTS_ALL Runtime)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
include(CPack)
