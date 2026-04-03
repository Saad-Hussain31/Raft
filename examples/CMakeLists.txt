# Example applications

add_executable(raft_node node_main.cpp)
target_link_libraries(raft_node PRIVATE raft_lib)

add_executable(raft_client client_main.cpp)
target_link_libraries(raft_client PRIVATE raft_lib)
