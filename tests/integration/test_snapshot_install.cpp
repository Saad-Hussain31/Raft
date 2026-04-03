#include <gtest/gtest.h>
#include <raft/raft.h>

// ---------------------------------------------------------------------------
// SnapshotInstall integration tests
// ---------------------------------------------------------------------------

// Verifies that the leader detects a follower whose nextIndex has fallen
// behind the leader's last included snapshot index and initiates an
// InstallSnapshot RPC to that follower.
TEST(SnapshotInstall, LeaderSendsSnapshotToLaggingFollower) {
    GTEST_SKIP() << "Not implemented";
}

// Sends a complete snapshot to a follower and confirms the follower replaces
// its state machine state, updates its last applied index, and discards any
// conflicting log entries preceding the snapshot.
TEST(SnapshotInstall, FollowerAppliesReceivedSnapshot) {
    GTEST_SKIP() << "Not implemented";
}

// Creates a snapshot large enough to require multiple chunks (offset-based
// InstallSnapshot RPCs) and verifies the follower reassembles and applies
// the full snapshot only after receiving the final chunk.
TEST(SnapshotInstall, SnapshotChunking) {
    GTEST_SKIP() << "Not implemented";
}
