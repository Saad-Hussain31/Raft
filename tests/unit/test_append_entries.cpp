#include <gtest/gtest.h>

#include "raft/core/raft_node.h"
#include "raft/rpc/messages.h"
#include "raft/util/types.h"

// An AppendEntries request whose term is lower than the receiver's
// currentTerm must be rejected (success = false).
TEST(AppendEntries, RejectIfTermLower) {
    GTEST_SKIP() << "Not implemented";
}

// An AppendEntries with an empty entries vector (a heartbeat) that has
// a valid term and matching prevLogIndex/prevLogTerm should be accepted
// (success = true) and should reset the election timer.
TEST(AppendEntries, AcceptHeartbeat) {
    GTEST_SKIP() << "Not implemented";
}

// New entries included in an AppendEntries request should be appended to
// the receiver's log at the correct indices.
TEST(AppendEntries, AppendNewEntries) {
    GTEST_SKIP() << "Not implemented";
}

// If an existing entry conflicts with a new one (same index, different
// term), the receiver must delete the existing entry and all that follow
// it, then append the new entries.
TEST(AppendEntries, TruncateConflictingEntries) {
    GTEST_SKIP() << "Not implemented";
}

// Re-appending the same entries that already exist in the log (same
// index and term) should be a no-op and succeed (idempotent).
TEST(AppendEntries, IdempotentAppend) {
    GTEST_SKIP() << "Not implemented";
}
