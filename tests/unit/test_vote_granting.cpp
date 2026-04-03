#include <gtest/gtest.h>

#include "raft/core/raft_node.h"
#include "raft/rpc/messages.h"
#include "raft/util/types.h"

// A node that has not voted in the current term (votedFor == nullopt)
// should grant a vote to a valid RequestVote request.
TEST(VoteGranting, GrantVoteIfNotVotedYet) {
    GTEST_SKIP() << "Not implemented";
}

// A node that has already voted for a different candidate in the current
// term must deny the vote.
TEST(VoteGranting, DenyVoteIfAlreadyVoted) {
    GTEST_SKIP() << "Not implemented";
}

// A node must deny a vote if the candidate's log is less up-to-date than
// its own (per the log comparison rule in Section 5.4.1).
TEST(VoteGranting, DenyVoteIfCandidateLogStale) {
    GTEST_SKIP() << "Not implemented";
}

// A node should grant a vote if the candidate's log is at least as
// up-to-date as the receiver's log (same or higher lastLogTerm, and
// same or higher lastLogIndex when terms are equal).
TEST(VoteGranting, GrantVoteIfCandidateLogUpToDate) {
    GTEST_SKIP() << "Not implemented";
}

// Granting a vote to the same candidate again in the same term is
// idempotent and should succeed (handles retransmissions).
TEST(VoteGranting, IdempotentVoteGrant) {
    GTEST_SKIP() << "Not implemented";
}
