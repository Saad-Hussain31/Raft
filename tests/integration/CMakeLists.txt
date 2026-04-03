add_executable(raft_integration_tests
    test_leader_election.cpp
    test_log_replication.cpp
    test_follower_catchup.cpp
    test_leader_failover.cpp
    test_stale_term.cpp
    test_snapshot_install.cpp
    test_network_partition.cpp
    test_restart_recovery.cpp
    test_membership_change.cpp
    test_client_interaction.cpp
)

target_link_libraries(raft_integration_tests
    PRIVATE
        raft_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(raft_integration_tests)
