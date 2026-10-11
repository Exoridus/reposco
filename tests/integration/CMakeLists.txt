qt_add_executable(integration_tests IntegrationTests.cpp)
find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
target_compile_definitions(integration_tests PRIVATE REPOSCO_TEST_PYTHON="${Python3_EXECUTABLE}")
target_link_libraries(integration_tests PRIVATE reposco_storage reposco_platform_windows Qt6::Test
                                                reposco_sqlcipher)
reposco_owned_target(integration_tests)
set_target_properties(integration_tests PROPERTIES WIN32_EXECUTABLE FALSE)
add_test(NAME integration.security COMMAND integration_tests -o -,txt)
set_tests_properties(integration.security PROPERTIES LABELS "integration;windows" TIMEOUT 60)
