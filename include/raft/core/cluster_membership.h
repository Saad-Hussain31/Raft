/**
 * @file cluster_membership.h
 * @brief Cluster membership tracking and joint-consensus configuration changes.
 *
 * Raft supports dynamic membership changes using the **joint consensus**
 * approach (§6 of the Raft paper).  A configuration change goes through two
 * phases:
 *
 *   1. C_old,new — the joint configuration.  Decisions require separate
 *      majorities from BOTH the old and new configurations.
 *   2. C_new — the new configuration.  Once C_new is committed, nodes not
 *      in C_new can shut down.
 *
 * Safety invariant: at no point can two leaders be elected for the same term.
 * The joint consensus approach guarantees this by requiring overlap between
 * old and new quorums during the transition.
 *
 * Implementation notes:
 *   - Configuration changes are replicated as special Config log entries.
 *   - A configuration change is "pending" from the moment a C_old,new entry
 *     is appended until a C_new entry is committed.
 *   - The leader must NOT begin a new configuration change while one is pending.
 *   - If the leader crashes during the transition, the new leader will see
 *     the C_old,new entry in its log and must complete the transition.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <unordered_set>
#include <vector>

#include "raft/util/types.h"

namespace raft {

/**
 * @brief Represents a single cluster configuration (a set of node IDs).
 */
struct ClusterConfig {
    std::unordered_set<NodeId> members;

    bool contains(NodeId id) const { return members.contains(id); }
    size_t size() const { return members.size(); }

    /**
     * @brief Returns the quorum size for this configuration.
     *
     * Quorum = floor(N/2) + 1, where N = |members|.
     */
    size_t quorum_size() const { return members.size() / 2 + 1; }
};

/**
 * @brief The phase of a membership change.
 */
enum class MembershipPhase : uint8_t {
    Stable,  ///< No change in progress; using a single configuration.
    Joint,   ///< Joint consensus: C_old,new is active.
};

/**
 * @brief Tracks the current cluster membership and any in-progress change.
 *
 * Thread safety: accessed only from the main Raft thread.
 */
class ClusterMembership {
   public:
    ClusterMembership() = default;

    /**
     * @brief Initialize with the given stable configuration.
     */
    explicit ClusterMembership(ClusterConfig initial);

    /**
     * @brief Return the current stable configuration.
     *
     * If a joint-consensus change is in progress, this returns C_old.
     */
    const ClusterConfig& current_config() const;

    /**
     * @brief Return the new configuration during a joint-consensus change.
     *
     * Returns nullopt if no change is in progress (phase == Stable).
     */
    std::optional<ClusterConfig> new_config() const;

    /** @brief Current phase (Stable or Joint). */
    MembershipPhase phase() const;

    /**
     * @brief Check whether a set of responding nodes constitutes a quorum.
     *
     * During Stable phase: requires a majority of current_config.
     * During Joint phase: requires SEPARATE majorities of BOTH C_old and C_new.
     *
     * @param responding_nodes  Set of node IDs that have responded favorably.
     * @return true if quorum is achieved.
     */
    bool has_quorum(const std::unordered_set<NodeId>& responding_nodes) const;

    /**
     * @brief Begin a joint-consensus membership change.
     *
     * Transitions from Stable → Joint.  The caller (RaftNode) must append a
     * Config log entry with the serialized C_old,new configuration.
     *
     * Precondition: phase() == Stable.
     * Postcondition: phase() == Joint; has_quorum() checks both configs.
     *
     * @param new_members  The desired new configuration (C_new).
     * @return Failure if a change is already in progress.
     */
    Result<void> begin_change(ClusterConfig new_members);

    /**
     * @brief Commit the joint configuration and transition to Stable(C_new).
     *
     * Called after the C_new entry is committed.  Nodes not in C_new can
     * now be shut down.
     *
     * Precondition: phase() == Joint.
     * Postcondition: phase() == Stable; current_config() == C_new.
     */
    Result<void> finalize_change();

    /**
     * @brief Abort a pending membership change, reverting to C_old.
     *
     * Used if the C_old,new entry is overwritten (e.g. leadership change).
     */
    void abort_change();

    /**
     * @brief All node IDs that should receive RPCs right now.
     *
     * During Stable: members of current config.
     * During Joint:  union of C_old and C_new.
     */
    std::unordered_set<NodeId> all_voting_members() const;

    /**
     * @brief Serialize the current configuration for a Config log entry.
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief Restore membership state from a serialized config.
     */
    static ClusterMembership deserialize(std::span<const uint8_t> data);

   private:
    ClusterConfig current_;
    std::optional<ClusterConfig> new_;
    MembershipPhase phase_{MembershipPhase::Stable};
};

}  // namespace raft
