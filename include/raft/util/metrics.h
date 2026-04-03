/**
 * @file metrics.h
 * @brief Metrics collection interface for Raft internals.
 *
 * Exposes counters and gauges that allow operators to monitor the health and
 * performance of a Raft node.  The default implementation stores values in
 * atomics; a production deployment can replace it with a Prometheus or
 * StatsD exporter.
 *
 * Key metrics:
 *   - elections_started        (counter) — how many elections this node began
 *   - elections_won            (counter) — how many elections this node won
 *   - vote_requests_received   (counter) — total RequestVote RPCs received
 *   - append_entries_received  (counter) — total AppendEntries RPCs received
 *   - snapshots_taken          (counter) — snapshots created
 *   - snapshots_installed      (counter) — InstallSnapshot RPCs applied
 *   - current_term             (gauge)   — this node's current term
 *   - commit_index             (gauge)   — highest committed log index
 *   - last_applied             (gauge)   — last index applied to state machine
 *   - log_size                 (gauge)   — number of entries in the log
 *   - role                     (gauge)   — 0=follower, 1=candidate, 2=leader
 *   - peers_count              (gauge)   — current cluster size
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string_view>

namespace raft {

/**
 * @brief Abstract metrics interface.
 *
 * Thread safety: all methods must be safe to call concurrently.
 */
class Metrics {
   public:
    virtual ~Metrics() = default;

    virtual void increment_counter(std::string_view name, uint64_t delta = 1) = 0;
    virtual void set_gauge(std::string_view name, int64_t value) = 0;
    virtual int64_t get_gauge(std::string_view name) const = 0;
    virtual uint64_t get_counter(std::string_view name) const = 0;
};

/**
 * @brief Creates an in-memory metrics store using atomic counters.
 *
 * Suitable for testing and single-process monitoring.  Replace with
 * a Prometheus or StatsD exporter for production.
 */
std::unique_ptr<Metrics> create_in_memory_metrics();

}  // namespace raft
