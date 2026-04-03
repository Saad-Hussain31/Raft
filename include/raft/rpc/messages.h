/**
 * @file messages.h
 * @brief RPC message definitions for the Raft protocol.
 *
 * Defines every message exchanged between Raft nodes, exactly matching the
 * RPCs described in §5 and §7 of the Raft paper:
 *
 *   1. RequestVote / RequestVoteReply          (§5.2 — Leader election)
 *   2. AppendEntries / AppendEntriesReply      (§5.3 — Log replication)
 *   3. InstallSnapshot / InstallSnapshotReply  (§7   — Log compaction)
 *
 * Additionally defines client-facing RPCs:
 *   4. ClientRequest / ClientResponse          (§8   — Client interaction)
 *
 * All messages are simple aggregate structs with no behaviour.
 * Serialization is handled separately by the Serializer component.
 */

#pragma once

#include <optional>
#include <vector>

#include "raft/util/types.h"

namespace raft::rpc {

// ─────────────────────────────────────────────────────────────────────────────
// §5.2  RequestVote RPC
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Sent by candidates to gather votes during an election.
 *
 * Fields (per paper Figure 2):
 *   term          — candidate's current term
 *   candidate_id  — candidate requesting the vote
 *   last_log_index — index of candidate's last log entry
 *   last_log_term  — term of candidate's last log entry
 *
 * Receiver rules:
 *   - Reply false if term < currentTerm.
 *   - Grant vote if votedFor is null or candidateId, AND candidate's log is
 *     at least as up-to-date as receiver's log (§5.4.1).
 *   - "At least as up-to-date" is defined by comparing (lastLogTerm, lastLogIndex)
 *     lexicographically.
 */
struct RequestVote {
    Term term;
    NodeId candidate_id;
    LogIndex last_log_index;
    Term last_log_term;
};

/**
 * @brief Response to a RequestVote RPC.
 *
 * term          — currentTerm of the voter, for candidate to update itself.
 * vote_granted  — true if the voter grants its vote.
 */
struct RequestVoteReply {
    Term term;
    bool vote_granted{false};
};

// ─────────────────────────────────────────────────────────────────────────────
// §5.3  AppendEntries RPC  (also serves as heartbeat when entries is empty)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Sent by the leader to replicate log entries and as a heartbeat.
 *
 * Fields (per paper Figure 2):
 *   term              — leader's current term
 *   leader_id         — so followers can redirect clients
 *   prev_log_index    — index of the log entry immediately preceding new ones
 *   prev_log_term     — term of the entry at prev_log_index
 *   entries           — log entries to store (empty for heartbeat)
 *   leader_commit     — leader's commitIndex
 *
 * Receiver rules:
 *   1. Reply false if term < currentTerm.
 *   2. Reply false if log doesn't contain an entry at prevLogIndex whose
 *      term matches prevLogTerm.
 *   3. If an existing entry conflicts with a new one (same index, different
 *      term), delete the existing entry and all that follow it.
 *   4. Append any new entries not already in the log.
 *   5. If leaderCommit > commitIndex, set commitIndex =
 *      min(leaderCommit, index of last new entry).
 *
 * Implementation note: entries may be empty (heartbeat).  The follower must
 * still check prev_log_index/prev_log_term and update commitIndex even for
 * heartbeats.
 */
struct AppendEntries {
    Term term;
    NodeId leader_id;
    LogIndex prev_log_index;
    Term prev_log_term;
    std::vector<LogEntry> entries;
    LogIndex leader_commit;
};

/**
 * @brief Response to an AppendEntries RPC.
 *
 * term     — currentTerm of the receiver, for leader to update itself.
 * success  — true if the follower contained an entry matching prevLogIndex
 *            and prevLogTerm.
 *
 * For the optimized log backtracking optimization (§5.3 optimization), the
 * follower may include:
 *   conflict_term   — the term of the conflicting entry (if any).
 *   conflict_index  — the first index the follower stores for conflict_term.
 *
 * These allow the leader to skip back an entire term at a time rather than
 * decrementing nextIndex one entry at a time.
 */
struct AppendEntriesReply {
    Term term;
    bool success{false};

    // ── Log backtracking optimization fields ──
    std::optional<Term> conflict_term;
    std::optional<LogIndex> conflict_index;
};

// ─────────────────────────────────────────────────────────────────────────────
// §7  InstallSnapshot RPC
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Sent by the leader to transfer a snapshot to a lagging follower.
 *
 * Used when the leader has already discarded the log entries a slow follower
 * needs.  The follower receiving the snapshot must:
 *   1. Reply immediately if term < currentTerm.
 *   2. Create/overwrite the snapshot file, writing data starting at the
 *      given byte offset.
 *   3. If offset == 0, create a new snapshot file.
 *   4. If done is true, discard all log entries up through
 *      last_included_index. If the snapshot covers a prefix of the existing
 *      log, retain entries after the snapshot; otherwise discard the entire log.
 *   5. Reset the state machine from the snapshot.
 *
 * Large snapshots are sent in chunks (offset + data + done flag).
 */
struct InstallSnapshot {
    Term term;
    NodeId leader_id;
    LogIndex last_included_index;
    Term last_included_term;
    uint64_t offset{0};
    std::vector<uint8_t> data;
    bool done{false};
};

/**
 * @brief Response to an InstallSnapshot RPC.
 *
 * term — currentTerm of the receiver, for leader to update itself.
 */
struct InstallSnapshotReply {
    Term term;
};

// ─────────────────────────────────────────────────────────────────────────────
// §8  Client interaction RPCs
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Command submitted by a client to be replicated.
 *
 * The client sends commands to the leader.  If the contacted node is not the
 * leader, it should respond with the leader hint so the client can retry.
 *
 * client_id + sequence_number together provide exactly-once semantics through
 * a client session table (§8 linearizable semantics).
 */
struct ClientRequest {
    uint64_t client_id{0};
    uint64_t sequence_number{0};
    std::vector<uint8_t> command;
};

/**
 * @brief Response to a client command.
 *
 * success      — true if the command was committed and applied.
 * leader_hint  — if the node is not the leader, the ID of the believed leader.
 * response     — the result from the state machine after applying the command.
 */
struct ClientResponse {
    bool success{false};
    ErrorCode error{ErrorCode::Ok};
    std::optional<NodeId> leader_hint;
    std::vector<uint8_t> response;
};

// ─────────────────────────────────────────────────────────────────────────────
// Unified message envelope
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Message type tag for dispatching incoming RPCs.
 */
enum class MessageType : uint8_t {
    RequestVote,
    RequestVoteReply,
    AppendEntries,
    AppendEntriesReply,
    InstallSnapshot,
    InstallSnapshotReply,
    ClientRequest,
    ClientResponse,
};

}  // namespace raft::rpc
