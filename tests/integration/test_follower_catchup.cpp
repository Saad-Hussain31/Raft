#include <gtest/gtest.h>
#include <raft/raft.h>

// ---------------------------------------------------------------------------
// FollowerCatchup integration tests
// ---------------------------------------------------------------------------

// Disconnects a follower while the leader continues to commit entries, then
// reconnects the follower and verifies it receives and applies all missed
// entries through normal AppendEntries RPCs.
TEST(FollowerCatchup, FollowerCatchesUpAfterTemporaryDisconnect) {
    GTEST_SKIP() << "Not implemented";
}

// Disconnects a follower long enough that the leader has compacted its log
// past the follower's last index, forcing recovery via InstallSnapshot RPC.
// Verifies the follower applies the snapshot and resumes normal replication.
TEST(FollowerCatchup, FollowerCatchesUpViaSnapshot) {
    GTEST_SKIP() << "Not implemented";
}
