#include <gtest/gtest.h>
#include <raft/raft.h>

// ---------------------------------------------------------------------------
// StaleTerm integration tests
// ---------------------------------------------------------------------------

// Sends an AppendEntries RPC with a term lower than the receiver's current
// term and verifies the receiver rejects it with success=false.
TEST(StaleTerm, RejectStaleTermAppendEntries) {
    GTEST_SKIP() << "Not implemented";
}

// Sends a RequestVote RPC with a term lower than the receiver's current term
// and verifies the receiver denies the vote.
TEST(StaleTerm, RejectStaleTermRequestVote) {
    GTEST_SKIP() << "Not implemented";
}

// Partitions a leader so it falls behind in term, then heals the partition.
// Verifies the stale leader observes a higher term in an incoming message
// and immediately steps down to follower state.
TEST(StaleTerm, StaleLeaderStepsDown) {
    GTEST_SKIP() << "Not implemented";
}
