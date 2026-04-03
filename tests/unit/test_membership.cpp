#include <gtest/gtest.h>

#include "raft/core/cluster_membership.h"
#include "raft/util/types.h"

// In a stable (non-joint) configuration, has_quorum should return true
// when a strict majority of members have responded.
TEST(Membership, StableQuorum) {
    GTEST_SKIP() << "Not implemented";
}

// During joint consensus (C_old,new), has_quorum must require separate
// majorities from BOTH the old and new configurations.
TEST(Membership, JointQuorumRequiresBothMajorities) {
    GTEST_SKIP() << "Not implemented";
}

// Calling begin_change with a new config should transition the
// membership phase from Stable to Joint.
TEST(Membership, BeginChangeTransitionsToJoint) {
    GTEST_SKIP() << "Not implemented";
}

// Calling finalize_change should transition from Joint to Stable with
// the new configuration (C_new) as the active config.
TEST(Membership, FinalizeChangeTransitionsToStable) {
    GTEST_SKIP() << "Not implemented";
}

// Calling abort_change during a joint-consensus transition should revert
// to the original configuration (C_old) and return to Stable phase.
TEST(Membership, AbortChangeRevertsToOld) {
    GTEST_SKIP() << "Not implemented";
}

// During joint consensus, all_voting_members should return the union of
// the old and new member sets.
TEST(Membership, AllVotingMembersDuringJoint) {
    GTEST_SKIP() << "Not implemented";
}
