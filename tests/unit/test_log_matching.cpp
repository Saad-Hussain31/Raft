#include <gtest/gtest.h>

#include "raft/core/raft_log.h"
#include "raft/util/types.h"

// An empty log should match the sentinel values (index 0, term 0),
// since there is no previous entry before the first real entry.
TEST(LogMatching, EmptyLogMatchesSentinel) {
    GTEST_SKIP() << "Not implemented";
}

// Calling match_term with the correct index and term for an existing
// entry should return true.
TEST(LogMatching, MatchTermSucceeds) {
    GTEST_SKIP() << "Not implemented";
}

// Calling match_term with the correct index but the wrong term should
// return false (the log matching property is violated).
TEST(LogMatching, MatchTermFailsOnWrongTerm) {
    GTEST_SKIP() << "Not implemented";
}

// Calling match_term with an index beyond the end of the log should
// return false (the entry does not exist).
TEST(LogMatching, MatchTermFailsOnMissingIndex) {
    GTEST_SKIP() << "Not implemented";
}
