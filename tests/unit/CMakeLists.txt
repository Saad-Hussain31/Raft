# Collect all unit test source files
file(GLOB UNIT_TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(raft_unit_tests ${UNIT_TEST_SOURCES})

target_link_libraries(raft_unit_tests
    PRIVATE
        raft_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(raft_unit_tests)
