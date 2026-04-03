# Source library

add_library(raft_lib STATIC
    # Core
    core/raft_node.cpp
    core/raft_log.cpp
    core/cluster_membership.cpp

    # RPC / Transport
    rpc/tcp_transport.cpp

    # Storage
    storage/file_log_store.cpp
    storage/file_snapshot_store.cpp
    storage/file_persistent_state.cpp

    # Utilities
    util/logger.cpp
    util/timer.cpp
    util/metrics.cpp
    util/serializer.cpp
)

target_include_directories(raft_lib
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(raft_lib
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
    PRIVATE
        pthread
)
