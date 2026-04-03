#include <gtest/gtest.h>

#include "raft/core/raft_node.h"
#include "raft/util/types.h"

// A follower that does not receive any AppendEntries or vote-granted RPCs
// before its election timer fires should transition to Candidate and start
// an election.
TEST(Election, ElectionTimeoutStartsElection) {
    GTEST_SKIP() << "Not implemented";
}

// When a follower transitions to Candidate it must increment its current
// term before sending RequestVote RPCs.
TEST(Election, CandidateIncrementsTerm) {
    GTEST_SKIP() << "Not implemented";
}

// A candidate must record a vote for itself at the start of the election
// (votedFor = self, vote count includes self).
TEST(Election, CandidateVotesForSelf) {
    GTEST_SKIP() << "Not implemented";
}

// In a single-node cluster the candidate already has a majority (1 of 1)
// and should immediately become Leader without waiting for external votes.
TEST(Election, SingleNodeWinsImmediately) {
    GTEST_SKIP() << "Not implemented";
}

// If no candidate receives a majority of votes (split vote), the election
// timer should fire again and a new election should begin with an
// incremented term.
TEST(Election, SplitVoteRestartsElection) {
    GTEST_SKIP() << "Not implemented";
}
