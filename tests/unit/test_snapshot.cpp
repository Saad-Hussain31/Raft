#include <gtest/gtest.h>

#include "raft/core/raft_node.h"
#include "raft/storage/snapshot_store.h"
#include "raft/util/types.h"

// A snapshot should be triggered automatically once the number of
// committed entries since the last snapshot exceeds the configured
// snapshot_threshold.
TEST(Snapshot, TriggerSnapshotAfterThreshold) {
    GTEST_SKIP() << "Not implemented";
}

// After a snapshot is taken, all log entries up to and including the
// snapshot's lastIncludedIndex should be removed from the log.
TEST(Snapshot, CompactLogAfterSnapshot) {
    GTEST_SKIP() << "Not implemented";
}

// Restoring from snapshot data must bring the state machine to the
// exact state captured at snapshot time, as if all entries up through
// lastIncludedIndex had been applied.
TEST(Snapshot, RestoreFromSnapshot) {
    GTEST_SKIP() << "Not implemented";
}

// Snapshot metadata (lastIncludedIndex, lastIncludedTerm, configuration)
// must accurately reflect the state at the time the snapshot was taken.
TEST(Snapshot, SnapshotMetadataConsistent) {
    GTEST_SKIP() << "Not implemented";
}
