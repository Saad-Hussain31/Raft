#include <gtest/gtest.h>
#include <raft/raft.h>

// ---------------------------------------------------------------------------
// LeaderFailover integration tests
// ---------------------------------------------------------------------------

// Stops the current leader and verifies that the remaining nodes hold a new
// election and successfully elect a replacement leader.
TEST(LeaderFailover, NewLeaderAfterOldLeaderDies) {
    GTEST_SKIP() << "Not implemented";
}

// After a leader change, submits a client request to a follower and verifies
// that the follower responds with a redirect hint pointing to the new leader.
TEST(LeaderFailover, ClientRequestsRedirectedToNewLeader) {
    GTEST_SKIP() << "Not implemented";
}

// Sends entries to a leader, kills it before those entries are committed to a
// majority, then verifies the new leader either re-commits or correctly
// overwrites the uncommitted entries, preserving Raft's safety guarantees.
TEST(LeaderFailover, UncommittedEntriesFromOldLeaderHandled) {
    GTEST_SKIP() << "Not implemented";
}
