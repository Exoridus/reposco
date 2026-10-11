qt_add_executable(integration_tests IntegrationTests.cpp)
find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
target_compile_definitions(integration_tests PRIVATE REPOSCO_TEST_PYTHON="${Python3_EXECUTABLE}")
target_link_libraries(integration_tests PRIVATE reposco_storage reposco_platform_windows Qt6::Test
                                                reposco_sqlcipher)
reposco_owned_target(integration_tests)
set_target_properties(integration_tests PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME integration.security COMMAND integration_tests -o -,txt)
set_tests_properties(integration.security PROPERTIES LABELS "integration;windows" TIMEOUT 60)

qt_add_executable(history_tests HistoryTests.cpp)
target_link_libraries(history_tests PRIVATE reposco_storage reposco_platform_windows Qt6::Test)
reposco_owned_target(history_tests)
set_target_properties(history_tests PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME integration.history COMMAND history_tests -o -,txt)
set_tests_properties(integration.history PROPERTIES LABELS "integration;windows" TIMEOUT 60)

qt_add_executable(native_capture_tests NativeCaptureTests.cpp)
target_link_libraries(native_capture_tests PRIVATE reposco_ui Qt6::Test)
reposco_owned_target(native_capture_tests)
set_target_properties(native_capture_tests PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME integration.native_capture
         COMMAND native_capture_tests inactiveListenerStartsAndStopsWithoutReading -o -,txt)
set_tests_properties(integration.native_capture PROPERTIES LABELS "integration;windows" TIMEOUT 15)
if(REPOSCO_ISOLATED_CLIPBOARD_TESTS)
  add_test(
    NAME integration.clipboard_isolated
    COMMAND
      native_capture_tests --isolated-clipboard syntheticUpdatesRespectMarkersPauseAndBackpressure
      exactCodesStayInPrivateClipboardAndOutOfHistory -o -,txt)
  set_tests_properties(integration.clipboard_isolated
                       PROPERTIES LABELS "integration;windows;privileged" TIMEOUT 30)
endif()

qt_add_executable(history_flow_tests HistoryFlowTests.cpp)
target_link_libraries(history_flow_tests PRIVATE reposco_ui Qt6::Test)
reposco_owned_target(history_flow_tests)
set_target_properties(history_flow_tests PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME integration.history_flow COMMAND history_flow_tests -o -,txt)
set_tests_properties(integration.history_flow PROPERTIES LABELS "integration;windows" TIMEOUT 60)
