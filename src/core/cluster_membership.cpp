/**
 * @file cluster_membership.cpp
 * @brief Cluster membership and joint-consensus implementation stubs.
 */

#include "raft/core/cluster_membership.h"

#include <algorithm>

namespace raft {

ClusterMembership::ClusterMembership(ClusterConfig initial) : current_{std::move(initial)} {}

const ClusterConfig& ClusterMembership::current_config() const {
    return current_;
}

std::optional<ClusterConfig> ClusterMembership::new_config() const {
    return new_;
}

MembershipPhase ClusterMembership::phase() const {
    return phase_;
}

bool ClusterMembership::has_quorum(const std::unordered_set<NodeId>& responding_nodes) const {
    // TODO: Check quorum under the current membership phase.
    //
    // Stable phase:
    //   Count how many members of current_ are in responding_nodes.
    //   Return count >= current_.quorum_size().
    //
    // Joint phase:
    //   Must have SEPARATE majorities of BOTH current_ (C_old) and new_ (C_new).
    //   size_t old_count = count responding_nodes ∩ current_.members;
    //   size_t new_count = count responding_nodes ∩ new_->members;
    //   Return old_count >= current_.quorum_size() && new_count >= new_->quorum_size();
    //
    // This is the fundamental safety mechanism for membership changes.
    return false;
}

Result<void> ClusterMembership::begin_change(ClusterConfig new_members) {
    // TODO: Transition from Stable → Joint.
    //
    //   1. If phase_ != Stable, return failure (change already in progress).
    //   2. new_ = std::move(new_members).
    //   3. phase_ = MembershipPhase::Joint.
    return Result<void>::failure(ErrorCode::NotImplemented);
}

Result<void> ClusterMembership::finalize_change() {
    // TODO: Transition from Joint → Stable(C_new).
    //
    //   1. If phase_ != Joint, return failure.
    //   2. current_ = *new_.
    //   3. new_ = std::nullopt.
    //   4. phase_ = MembershipPhase::Stable.
    return Result<void>::failure(ErrorCode::NotImplemented);
}

void ClusterMembership::abort_change() {
    // TODO: Revert to C_old.
    //
    //   new_ = std::nullopt;
    //   phase_ = MembershipPhase::Stable;
}

std::unordered_set<NodeId> ClusterMembership::all_voting_members() const {
    // TODO: Return the union of all members that should receive RPCs.
    //
    // Stable: return current_.members.
    // Joint:  return current_.members ∪ new_->members.
    return current_.members;
}

std::vector<uint8_t> ClusterMembership::serialize() const {
    // TODO: Serialize the current membership state to bytes.
    //
    // Include:
    //   - phase
    //   - current_ member IDs
    //   - new_ member IDs (if Joint)
    //
    // Use the JSON serializer or a simple binary format.
    return {};
}

ClusterMembership ClusterMembership::deserialize(std::span<const uint8_t> /*data*/) {
    // TODO: Deserialize membership state from bytes.
    return ClusterMembership{};
}

}  // namespace raft
