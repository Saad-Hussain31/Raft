/**
 * @file configuration.h
 * @brief Node configuration and startup parameters.
 *
 * Centralizes all tuneable parameters for a Raft node.  Sensible defaults
 * are provided, but every value can be overridden for testing or deployment.
 *
 * Configuration is immutable after construction — the RaftNode reads it once
 * at startup.  Runtime changes to cluster membership are handled separately
 * through the ClusterMembership module and Config log entries.
 */

#pragma once

#include <string>
#include <vector>

#include "raft/util/logger.h"
#include "raft/util/timer.h"
#include "raft/util/types.h"

namespace raft {

/**
 * @brief Describes a single peer in the initial cluster configuration.
 */
struct PeerConfig {
    NodeId id;
    PeerAddress address;
};

/**
 * @brief Complete configuration for a Raft node.
 */
struct Configuration {
    // ── Identity ────────────────────────────────────────────────────────

    /** @brief This node's unique ID.  Must be stable across restarts. */
    NodeId node_id;

    /** @brief Cluster identifier.  Prevents cross-cluster communication. */
    ClusterId cluster_id;

    /** @brief The address this node listens on. */
    PeerAddress listen_address;

    // ── Cluster peers ───────────────────────────────────────────────────

    /**
     * @brief Initial cluster membership.
     *
     * This is the bootstrap configuration.  After the first configuration
     * change is committed, the cluster membership is determined by the log.
     * Include this node in the list.
     */
    std::vector<PeerConfig> initial_peers;

    // ── Timing ──────────────────────────────────────────────────────────

    TimerConfig timers;

    // ── Storage paths ───────────────────────────────────────────────────

    /**
     * @brief Root directory for all persistent data.
     *
     * Subdirectories will be created for:
     *   data_dir/log/        — log entries
     *   data_dir/snapshots/  — snapshots
     *   data_dir/state/      — currentTerm + votedFor
     */
    std::string data_dir{"./raft_data"};

    // ── Snapshotting ────────────────────────────────────────────────────

    /**
     * @brief Take a snapshot after this many new entries have been committed
     *        since the last snapshot.
     *
     * Set to 0 to disable automatic snapshotting (manual only).
     */
    uint64_t snapshot_threshold{10000};

    /**
     * @brief Maximum size in bytes of a single InstallSnapshot chunk.
     */
    uint64_t snapshot_chunk_size{1024 * 1024};  // 1 MiB

    // ── Networking ──────────────────────────────────────────────────────

    /**
     * @brief Maximum number of in-flight AppendEntries RPCs per peer.
     *
     * Pipelining AppendEntries improves throughput on high-latency links.
     * Set to 1 for strict serial replication.
     */
    uint32_t max_inflight_rpcs{1};

    /**
     * @brief RPC timeout for a single request.
     *
     * If a peer does not respond within this duration, the RPC is considered
     * failed and may be retried.
     */
    std::chrono::milliseconds rpc_timeout{100};

    // ── Logging ─────────────────────────────────────────────────────────

    LogLevel log_level{LogLevel::Info};
};

}  // namespace raft
