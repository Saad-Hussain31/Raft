#include <gtest/gtest.h>

#include "raft/storage/log_store.h"
#include "raft/storage/persistent_state.h"
#include "raft/util/types.h"

// After saving currentTerm and votedFor, reloading the PersistentState
// from the same directory must return the identical values.
TEST(Persistence, PersistTermAndVotedFor) {
    GTEST_SKIP() << "Not implemented";
}

// A partial/interrupted write (simulated crash mid-save) must not leave
// the persistent state in a corrupt state; the previous valid state
// should be recoverable.
TEST(Persistence, AtomicPersistence) {
    GTEST_SKIP() << "Not implemented";
}

// After a simulated crash and restart, recovery should read back the
// most recent durably persisted term and votedFor.
TEST(Persistence, RecoverAfterCrash) {
    GTEST_SKIP() << "Not implemented";
}

// Log entries appended and synced via the LogStore must be present
// after constructing a new LogStore instance over the same directory
// (simulating a restart).
TEST(Persistence, LogSurvivesRestart) {
    GTEST_SKIP() << "Not implemented";
}
