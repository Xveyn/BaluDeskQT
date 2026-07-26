find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(baludesk_core_tests
    core/app_config_test.cpp
)

target_link_libraries(baludesk_core_tests PRIVATE
    baludesk_core
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(baludesk_core_tests)
