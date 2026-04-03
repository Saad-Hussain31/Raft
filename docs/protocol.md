# Raft Protocol

This document describes the Raft consensus protocol as implemented in this codebase. All section references (e.g., "Section 5.2") refer to the extended Raft paper: [In Search of an Understandable Consensus Algorithm (Extended Version)](https://raft.github.io/raft.pdf) by Diego Ongaro and John Ousterhout.

## Table of Contents

- [Fundamentals](#fundamentals)
- [Leader Election](#leader-election)
- [Log Replication](#log-replication)
- [Commit Rules](#commit-rules)
- [Log Compaction and Snapshotting](#log-compaction-and-snapshotting)
- [InstallSnapshot RPC](#installsnapshot-rpc)
- [Cluster Membership Changes](#cluster-membership-changes)
- [Client Interaction](#client-interaction)
- [Safety Properties and Invariants](#safety-properties-and-invariants)

---

## Fundamentals

### Terms (Section 5.1)

Time in Raft is divided into **terms** of arbitrary length. Each term begins with an election. Terms act as a logical clock:

- Every RPC includes the sender's term.
- If a server receives an RPC with a term greater than its own, it updates its term and converts to Follower.
- If a server receives an RPC with a stale term (less than its own), it rejects the RPC.
- Terms never decrease.

Implementation: `Term` is a strongly-typed `uint64_t` wrapper. The `maybe_step_down(Term)` method in `RaftNode` implements the universal term-check rule.

### Roles (Section 5.1)

Each server is in exactly one of three states:

- **Follower**: Passive. Responds to RPCs from leaders and candidates. If it doesn't hear from a leader before the election timeout, it becomes a Candidate.
- **Candidate**: Actively seeking votes. Sends RequestVote RPCs to all peers. Becomes Leader on majority vote, reverts to Follower on discovering a higher term or a new leader.
- **Leader**: Handles all client requests. Replicates log entries. Sends periodic heartbeats.

Implementation: `Role` is an enum in `types.h`. Transitions are managed by `become_follower()`, `start_election()`, and `become_leader()` in `RaftNode`.

### Log Entries (Section 5.3)

The log is an ordered sequence of entries. Each entry contains:

- **index**: 1-based position in the log.
- **term**: The term in which the entry was created.
- **type**: Command (client data), Noop (leader marker), or Config (membership change).
- **data**: Opaque byte vector. Only the StateMachine interprets Command data.

Implementation: `LogEntry` struct in `types.h`. Managed by `RaftLog` with `LogStore` backend.

---

## Leader Election

**Paper Reference**: Section 5.2

### Election Trigger

A follower starts an election when its **election timer** expires without receiving:
- An AppendEntries RPC from a valid leader, or
- A vote grant from a candidate it voted for.

The election timeout is randomly chosen from `[election_timeout_min, election_timeout_max]` (typically 150-300ms). Randomization prevents split votes.

### Election Procedure

When starting an election, a server (`start_election()`):

1. Increments `currentTerm`.
2. Transitions to Candidate.
3. Votes for itself (`votedFor = self`).
4. Persists `currentTerm` and `votedFor` to durable storage.
5. Resets the election timer with a new random duration.
6. Sends `RequestVote` RPCs to all other servers in the cluster.

### RequestVote RPC (Section 5.2, 5.4.1)

**Request fields** (per Figure 2):

| Field            | Description                          |
|------------------|--------------------------------------|
| `term`           | Candidate's current term             |
| `candidate_id`   | Candidate requesting vote            |
| `last_log_index` | Index of candidate's last log entry  |
| `last_log_term`  | Term of candidate's last log entry   |

**Receiver implementation** (`handle_request_vote()`):

1. If `request.term < currentTerm`, reply `vote_granted = false`.
2. If already voted for a different candidate in this term, reply false.
3. Check the **election restriction** (Section 5.4.1): the candidate's log must be "at least as up-to-date" as the receiver's log. Comparison is lexicographic on `(lastLogTerm, lastLogIndex)`:
   - Higher last-log-term wins.
   - Same last-log-term: higher last-log-index wins.
4. If the candidate passes all checks:
   - Set `votedFor = candidate_id`.
   - **Persist** before replying.
   - Reset the election timer.
   - Reply `vote_granted = true`.

**Reply fields**:

| Field          | Description                               |
|----------------|-------------------------------------------|
| `term`         | Voter's currentTerm (for candidate to update) |
| `vote_granted` | True if vote is granted                   |

### Winning an Election

A candidate wins when it receives votes from a **majority** of the cluster (`handle_request_vote_reply()`). On winning:

1. Transition to Leader (`become_leader()`).
2. Initialize `nextIndex[peer] = lastLogIndex + 1` for all peers.
3. Initialize `matchIndex[peer] = 0` for all peers.
4. Append a **Noop entry** with the current term (Section 8).
5. Stop the election timer.
6. Start the heartbeat timer.
7. Immediately send AppendEntries to all peers.

### Election Timeout Handling

If the election timer expires while already a Candidate (split vote), the candidate starts a new election with an incremented term. The randomized timeout makes sustained split votes unlikely.

---

## Log Replication

**Paper Reference**: Section 5.3

### AppendEntries RPC

The leader replicates log entries using AppendEntries RPCs. These also serve as heartbeats when the `entries` field is empty.

**Request fields** (per Figure 2):

| Field             | Description                                         |
|-------------------|-----------------------------------------------------|
| `term`            | Leader's current term                               |
| `leader_id`       | So followers can redirect clients                   |
| `prev_log_index`  | Index of entry immediately preceding new entries    |
| `prev_log_term`   | Term of the entry at `prev_log_index`               |
| `entries`         | Log entries to replicate (empty for heartbeat)      |
| `leader_commit`   | Leader's commit index                               |

**Receiver implementation** (`handle_append_entries()`):

1. If `request.term < currentTerm`, reply `success = false`.
2. Recognize the sender as the leader. Reset the election timer.
3. **Log consistency check**: if the log does not contain an entry at `prev_log_index` with term `prev_log_term`, reply `success = false` with conflict information for fast backtracking.
4. **Process entries**:
   - For each entry at `prev_log_index + 1 + i`:
     - If the log already has an entry at that index with the **same term**, skip it (idempotent).
     - If the log has an entry at that index with a **different term**, delete it and all following entries, then append the new entries.
     - If the log has no entry at that index, append.
5. **Update commit index**: if `leader_commit > commitIndex`, set `commitIndex = min(leader_commit, index of last new entry)`.
6. Reply `success = true`.

**Reply fields**:

| Field            | Description                                              |
|------------------|----------------------------------------------------------|
| `term`           | Receiver's currentTerm                                   |
| `success`        | True if log matched at prev_log_index/prev_log_term      |
| `conflict_term`  | (Optimization) Term of the conflicting entry             |
| `conflict_index` | (Optimization) First index for conflict_term             |

### Fast Log Backtracking Optimization (Section 5.3)

When AppendEntries fails due to a log inconsistency, the naive approach decrements `nextIndex` by 1 and retries. The optimized approach allows skipping entire terms at once:

- The follower includes `conflict_term` (the term of the conflicting entry) and `conflict_index` (the first index it stores for that term).
- The leader searches its own log for the last entry with `conflict_term`:
  - If found: `nextIndex = that index + 1`.
  - If not found: `nextIndex = conflict_index`.

Implementation: `handle_append_entries_reply()` in `RaftNode`.

### Leader Sending Logic

`send_append_entries(peer_id)`:

1. Look up `nextIndex[peer]` from `peer_states_`.
2. Compute `prevLogIndex = nextIndex - 1` and `prevLogTerm = log.term_at(prevLogIndex)`.
3. Collect entries from `nextIndex` through `lastLogIndex`.
4. Build and send the AppendEntries message.
5. If `nextIndex` has fallen below the snapshot's `last_included_index`, send InstallSnapshot instead.

`send_append_entries_to_all()`:

- Called on heartbeat timeout and after appending new entries.
- Iterates over all peers and calls `send_append_entries()` or `send_install_snapshot()` as appropriate.
- Resets the heartbeat timer.

---

## Commit Rules

**Paper Reference**: Section 5.3, 5.4.2

### Advancing the Commit Index

A log entry is **committed** once the leader that created it has replicated it to a majority of servers. The leader tracks this via `matchIndex[peer]` for each follower:

`advance_commit_index()`:

1. Find the largest N > commitIndex such that:
   - A majority of `matchIndex[i] >= N` (including the leader itself).
   - `log[N].term == currentTerm`.
2. Set `commitIndex = N`.

### The Current-Term Restriction (Section 5.4.2)

**Critical safety rule**: A leader MUST NOT commit entries from previous terms by counting replicas. It can only directly commit entries from its **own** term. Once a current-term entry is committed, all preceding entries are committed indirectly (by the Log Matching property).

This is why a new leader appends a **Noop entry**: to ensure there is always a current-term entry that can be committed, which in turn commits all prior entries.

Without this restriction, the scenario in Figure 8 of the paper can violate safety.

### Applying Committed Entries

`apply_committed_entries()`:

```
while lastApplied < commitIndex:
    lastApplied++
    entry = log.entry_at(lastApplied)
    result = state_machine.apply(entry)

    if entry has pending client callback:
        invoke callback with result

    if entry is a Config entry:
        handle membership transition
```

Committed entries are applied exactly once, in index order, to the StateMachine.

---

## Log Compaction and Snapshotting

**Paper Reference**: Section 7

### Why Snapshots

Without compaction, the log grows without bound. Snapshots capture the entire state machine state at a point in time, allowing all preceding log entries to be discarded.

### Taking a Snapshot

`maybe_take_snapshot()`:

1. Check if `entries_since_last_snapshot >= snapshot_threshold`.
2. If yes:
   a. Call `state_machine.take_snapshot()` to capture the current state.
   b. Build `SnapshotMetadata{last_included_index, last_included_term, configuration}`.
   c. Write the snapshot to the SnapshotStore.
   d. Compact the log: discard all entries with index <= `last_included_index`.

### Snapshot Metadata

Each snapshot records:
- `last_included_index`: The index of the last entry included in the snapshot.
- `last_included_term`: The term of that entry.
- `configuration`: The serialized cluster membership at snapshot time (needed for safe recovery even if the Config entry has been compacted).

---

## InstallSnapshot RPC

**Paper Reference**: Section 7

When a follower is so far behind that the leader has already compacted the required log entries, the leader sends a snapshot via the InstallSnapshot RPC.

### Request Fields

| Field                  | Description                                      |
|------------------------|--------------------------------------------------|
| `term`                 | Leader's current term                            |
| `leader_id`            | For follower to redirect clients                 |
| `last_included_index`  | Snapshot replaces all entries through this index  |
| `last_included_term`   | Term of the entry at last_included_index         |
| `offset`               | Byte offset in the snapshot data                 |
| `data`                 | Chunk of snapshot data                           |
| `done`                 | True if this is the last chunk                   |

### Receiver Implementation (`handle_install_snapshot()`)

1. If `request.term < currentTerm`, reply with current term.
2. Reset election timer.
3. If we already have a snapshot with a higher `last_included_index`, ignore (stale).
4. If `offset == 0`, begin a new snapshot in the SnapshotStore.
5. Write the chunk data at the given offset.
6. If `done == false`, reply and wait for more chunks.
7. If `done == true`:
   a. Finalize the snapshot.
   b. Restore the state machine from the snapshot data.
   c. Update the log: if the log contains an entry at `last_included_index` with matching term, keep entries after it; otherwise discard the entire log.
   d. Update `lastApplied` and `commitIndex`.

### Reply Fields

| Field  | Description                                          |
|--------|------------------------------------------------------|
| `term` | Receiver's currentTerm (for leader to step down)     |

### Leader-Side Tracking

The leader tracks the `snapshot_offset` per peer in `PeerState`. After the final chunk is acknowledged, it updates `nextIndex` and `matchIndex` and resumes normal AppendEntries replication.

---

## Cluster Membership Changes

**Paper Reference**: Section 6

### Joint Consensus Approach

This implementation uses the **joint consensus** approach for safe membership changes. A configuration change proceeds in two phases:

**Phase 1: C_old,new (Joint Configuration)**

1. The leader appends a Config entry containing the union of the old and new configurations.
2. From this point, decisions require **separate majorities from both** C_old and C_new.
3. This entry is replicated to all servers in the union.

**Phase 2: C_new (New Configuration)**

1. Once C_old,new is committed, the leader appends a Config entry containing only C_new.
2. Once C_new is committed, servers not in C_new can be shut down.
3. The cluster is now operating under the new configuration.

### Safety Guarantee

The joint consensus approach ensures that at no point can two leaders be elected for the same term. During the transition, any majority must overlap with at least one server from the previous configuration.

### Implementation

`ClusterMembership` manages the configuration state:

- `begin_change(new_config)`: Stable -> Joint. Stores both C_old and C_new.
- `has_quorum(votes)`: In Joint phase, requires separate majorities of both configs.
- `finalize_change()`: Joint -> Stable(C_new). Drops C_old.
- `abort_change()`: Reverts to C_old if the C_old,new entry is overwritten.

### Restrictions

- Only one configuration change may be in progress at a time.
- The leader must not begin a new change while one is pending.
- If the leader crashes during the transition, the new leader will see the C_old,new entry and must complete the transition.

---

## Client Interaction

**Paper Reference**: Section 8

### Submitting Commands

Clients submit commands to the leader via `submit_command()`. If the contacted node is not the leader, it responds with a `leader_hint` so the client can retry with the correct node.

### Exactly-Once Semantics

To prevent duplicate application of client commands (due to client retries after timeouts), the implementation maintains a **client session table**:

- Each client is assigned a `client_id`.
- Each command carries a `sequence_number`.
- The leader tracks `(client_id, last_sequence_number, last_response)`.
- If a command arrives with a `sequence_number <= last_sequence_number` for that client, the cached response is returned without re-applying.

This is stored in `client_sessions_` in `RaftNode`.

### Linearizability

For linearizable reads, clients must go through the leader. The leader must verify it is still the leader before responding to a read. Options:

1. **Log-based reads**: Treat reads as commands that go through the log (simple but adds latency).
2. **ReadIndex** (not yet implemented): The leader records the current commit index, confirms leadership with a round of heartbeats, then serves the read once `lastApplied >= that index`.
3. **Lease-based reads** (not yet implemented): The leader maintains a lease based on heartbeat responses, serving reads without extra RPCs within the lease period.

### Client Response

The `ClientResponse` struct contains:

| Field         | Description                                   |
|---------------|-----------------------------------------------|
| `success`     | True if the command was committed and applied  |
| `error`       | Error code (NotLeader, Timeout, etc.)          |
| `leader_hint` | ID of the believed leader (for redirect)       |
| `response`    | Result bytes from the state machine            |

---

## Safety Properties and Invariants

The Raft protocol guarantees five safety properties. A correct implementation must maintain all of them.

### 1. Election Safety (Section 5.2)

> At most one leader can be elected in a given term.

**How maintained**: Each server votes at most once per term (persisted in `votedFor`). A leader requires a majority of votes. Two majorities must overlap in at least one server, which cannot have voted for two different candidates.

### 2. Leader Append-Only (Section 5.3)

> A leader never overwrites or deletes entries in its log; it only appends new entries.

**How maintained**: Only followers execute `truncate_from()`. The leader only calls `append()`.

### 3. Log Matching (Section 5.3)

> If two logs contain an entry with the same index and term, then the logs are identical in all entries up through that index.

**How maintained**: The AppendEntries consistency check. Each AppendEntries includes `prevLogIndex` and `prevLogTerm`. The follower rejects the RPC if its log doesn't match at that point, causing the leader to back up until the logs agree.

### 4. Leader Completeness (Section 5.4)

> If a log entry is committed in a given term, that entry will be present in the logs of the leaders for all higher-numbered terms.

**How maintained**: The election restriction (Section 5.4.1). Voters deny their vote to candidates whose logs are less up-to-date. Since a committed entry is on a majority, and a new leader must get votes from a majority, the new leader's log must contain the committed entry.

### 5. State Machine Safety (Section 5.4.3)

> If a server has applied a log entry at a given index to its state machine, no other server will ever apply a different log entry for that same index.

**How maintained**: By the combination of Log Matching and Leader Completeness. Once an entry is committed (agreed by a majority), it is permanent. The leader's commit rule (current-term restriction) prevents the scenario where an uncommitted entry could be overwritten after being partially replicated.

### Critical Implementation Invariants

These invariants must be maintained by the implementation for safety:

1. **Terms never decrease**: `currentTerm` is monotonically increasing on each server.
2. **At most one vote per term**: `votedFor` is set at most once per term (null -> candidateId). Same candidate votes are idempotent.
3. **Persist before reply**: `currentTerm`, `votedFor`, and log entries must be durably stored before any RPC response is sent.
4. **Atomic term+vote**: `currentTerm` and `votedFor` must be persisted atomically (a crash between the two can violate Election Safety).
5. **Commit index never decreases**: `commitIndex` is monotonically increasing.
6. **Entries applied in order**: `lastApplied` increases by 1 at a time, and `lastApplied <= commitIndex`.
7. **Current-term commit rule**: The leader only commits entries from its own term directly. Previous-term entries are committed indirectly.
