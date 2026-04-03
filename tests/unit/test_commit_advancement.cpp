#include <gtest/gtest.h>

#include "raft/core/raft_node.h"
#include "raft/util/types.h"

// The leader should advance commitIndex to N when a majority of
// matchIndex values are >= N and log[N].term == currentTerm.
TEST(CommitAdvancement, AdvanceOnMajorityMatch) {
    GTEST_SKIP() << "Not implemented";
}

// A leader must NOT directly commit entries from previous terms by
// counting replicas; it can only commit them indirectly by first
// committing an entry from its own term (the Noop, per Section 5.4.2).
TEST(CommitAdvancement, OnlyCommitCurrentTermEntries) {
    GTEST_SKIP() << "Not implemented";
}

// The commitIndex must never decrease, even if the set of responding
// peers changes.
TEST(CommitAdvancement, CommitIndexNeverDecreases) {
    GTEST_SKIP() << "Not implemented";
}

// Entries between lastApplied+1 and commitIndex must be applied to the
// state machine in strictly increasing index order.
TEST(CommitAdvancement, ApplyEntriesInOrder) {
    GTEST_SKIP() << "Not implemented";
}
