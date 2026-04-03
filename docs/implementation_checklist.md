# Implementation Checklist

Every TODO in the codebase, organized by module. Each item describes what needs to be implemented and references the relevant section of the [Raft paper](https://raft.github.io/raft.pdf).

**Legend:**
- `[ ]` Not started
- `[~]` Partially implemented
- `[x]` Complete

---

## Core Module

### RaftNode (`src/core/raft_node.cpp`)

#### Lifecycle

- [ ] **`start()`** -- Full startup sequence.
  - Recover persistent state (currentTerm, votedFor) from `PersistentState`.
  - Recover log from `LogStore` (already loaded by `RaftLog` constructor).
  - Restore state machine from snapshot if one exists.
  - Initialize cluster membership from `config_.initial_peers` or latest Config log entry.
  - Start transport with message handler that enqueues `IncomingMessage` events.
  - Add all peers to transport.
  - Create election and heartbeat timers via `TimerFactory`.
  - Reset election timer (start as Follower).
  - Launch event loop thread.
  - Set `running_ = true`.
  - **Paper**: Section 5.1 (server startup), Figure 2 (state initialization).

- [ ] **`stop()`** -- Graceful shutdown.
  - Guard against double-stop (idempotent).
  - Set `running_ = false`.
  - Post `ShutdownEvent` to event queue.
  - Join event loop thread.
  - Stop timers and transport.
  - Fail pending client requests with `ShuttingDown`.
  - **Paper**: N/A (operational concern).

#### Event Loop

- [ ] **`run_event_loop()`** -- Main event processing loop.
  - Wait on condition variable for events.
  - Dispatch events via `std::visit` on the `Event` variant.
  - Call `apply_committed_entries()` and `maybe_take_snapshot()` after each event.
  - Exit on `ShutdownEvent`.
  - **Paper**: N/A (implementation design).

- [ ] **`enqueue(Event)`** -- Thread-safe event posting.
  - Lock `queue_mutex_`, push event, notify `queue_cv_`.
  - **Paper**: N/A (implementation design).

#### Message Dispatch

- [ ] **`dispatch_message(IncomingMessage)`** -- Route incoming RPCs.
  - Switch on `msg.type` and call the appropriate deserialize + handler method.
  - Handle deserialization errors gracefully (log warning, do not crash).
  - **Paper**: N/A (implementation design).

#### RequestVote Handling

- [ ] **`handle_request_vote(NodeId, RequestVote)`** -- Process incoming RequestVote.
  - Call `maybe_step_down()`.
  - Check term, existing vote, and log up-to-date condition.
  - Persist `votedFor` before replying.
  - Reset election timer on grant.
  - Send reply via transport.
  - Update metrics.
  - **Paper**: Section 5.2 (election), Section 5.4.1 (election restriction), Figure 2.

- [ ] **`handle_request_vote_reply(NodeId, RequestVoteReply)`** -- Process vote responses.
  - Ignore if not Candidate or stale term.
  - Call `maybe_step_down()`.
  - Track votes in `votes_received_`.
  - On quorum, call `become_leader()`.
  - **Paper**: Section 5.2.

#### AppendEntries Handling

- [ ] **`handle_append_entries(NodeId, AppendEntries)`** -- Process incoming AppendEntries/heartbeat.
  - Call `maybe_step_down()`.
  - Validate term.
  - Recognize leader, reset election timer.
  - Log consistency check with fast backtracking info.
  - Process entries: skip matching, truncate conflicting, append new.
  - Update commit index.
  - Handle Config entries.
  - Update metrics.
  - **Paper**: Section 5.3, Figure 2 (AppendEntries receiver implementation).

- [ ] **`handle_append_entries_reply(NodeId, AppendEntriesReply)`** -- Process replication responses.
  - Ignore if not Leader or stale term.
  - Call `maybe_step_down()`.
  - On success: update `matchIndex`, `nextIndex`, call `advance_commit_index()`.
  - On failure: fast backtracking using `conflict_term`/`conflict_index`, or decrement `nextIndex`. Switch to `send_install_snapshot()` if needed.
  - **Paper**: Section 5.3 (log backtracking optimization).

#### InstallSnapshot Handling

- [ ] **`handle_install_snapshot(NodeId, InstallSnapshot)`** -- Process incoming snapshot.
  - Call `maybe_step_down()`.
  - Validate term, reset election timer.
  - Check for stale snapshot.
  - Write chunks via `SnapshotStore`.
  - On final chunk: finalize, restore state machine, update log, update volatile state.
  - **Paper**: Section 7 (log compaction), Figure 13.

- [ ] **`handle_install_snapshot_reply(NodeId, InstallSnapshotReply)`** -- Process snapshot acknowledgments.
  - Step down if higher term.
  - On final chunk acknowledged: update `nextIndex`/`matchIndex`, resume AppendEntries.
  - On partial: send next chunk.
  - **Paper**: Section 7.

#### Role Transitions

- [ ] **`become_follower(Term, optional<NodeId>)`** -- Transition to Follower.
  - Update role, term, votedFor. Persist.
  - Stop heartbeat timer, reset election timer.
  - Clear peer_states_ and votes_received_.
  - Fail pending client requests.
  - Update metrics.
  - **Paper**: Section 5.1 (rules for servers).

- [ ] **`start_election()`** -- Start an election.
  - Increment term, vote for self, persist.
  - Reset votes, check single-node quorum.
  - Reset election timer.
  - Send RequestVote to all peers.
  - Update metrics.
  - **Paper**: Section 5.2.

- [ ] **`become_leader()`** -- Transition to Leader.
  - Assert Candidate role.
  - Initialize peer_states_ (nextIndex, matchIndex).
  - Append Noop entry.
  - Stop election timer, start heartbeat timer.
  - Immediately send AppendEntries to all.
  - Update metrics.
  - **Paper**: Section 5.2 (leader initialization), Section 8 (Noop entry), Figure 2.

#### Leader Operations

- [ ] **`send_append_entries_to_all()`** -- Broadcast AppendEntries to all peers.
  - Iterate over peers, call `send_append_entries()` or `send_install_snapshot()`.
  - Reset heartbeat timer.
  - **Paper**: Section 5.3.

- [ ] **`send_append_entries(NodeId)`** -- Send AppendEntries to a single peer.
  - Compute prevLogIndex/prevLogTerm from peer state.
  - Collect entries from log.
  - Build, serialize, and send the message.
  - **Paper**: Section 5.3, Figure 2.

- [ ] **`send_install_snapshot(NodeId)`** -- Send next snapshot chunk to a peer.
  - Read chunk from SnapshotStore.
  - Determine if this is the last chunk.
  - Build, serialize, and send the message.
  - Update peer's snapshot_offset.
  - **Paper**: Section 7.

- [ ] **`advance_commit_index()`** -- Try to advance commitIndex.
  - Find largest N > commitIndex with majority matchIndex >= N and log[N].term == currentTerm.
  - Update metrics.
  - **Paper**: Section 5.3, Section 5.4.2 (commit restriction).

- [ ] **`apply_committed_entries()`** -- Apply entries to the state machine.
  - Walk from `lastApplied + 1` through `commitIndex`.
  - Call `state_machine_->apply()` for each.
  - Invoke pending client callbacks.
  - Handle Config entries (joint consensus transitions).
  - Check client sessions for exactly-once semantics.
  - Update metrics.
  - **Paper**: Section 5.3 (rules for all servers), Section 8 (client interaction).

#### Client Interface

- [ ] **`submit_command(ClientRequest, callback)`** -- Accept a client command.
  - Reject with `NotLeader` if not leader.
  - Check client session for duplicate detection.
  - Append Command entry to log.
  - Store pending request.
  - Trigger replication to all peers.
  - **Paper**: Section 8.

#### Snapshotting

- [ ] **`maybe_take_snapshot()`** -- Auto-snapshot check.
  - Check threshold against entries since last snapshot.
  - Capture state machine state.
  - Write snapshot to SnapshotStore.
  - Compact log.
  - Update metrics.
  - **Paper**: Section 7.

#### Term Management

- [ ] **`maybe_step_down(Term)`** -- Universal term-check rule.
  - If received term > currentTerm, become follower with new term.
  - **Paper**: Section 5.1 (rules for servers).

#### Helpers

- [ ] **`random_election_timeout()`** -- Generate random duration in configured range.
  - Use thread-local random engine.
  - **Paper**: Section 5.2 (randomized timeouts).

- [ ] **`reset_election_timer()`** -- Reset election timer with random duration.
  - Call `election_timer_->reset()` with callback that enqueues `ElectionTimeoutEvent`.
  - **Paper**: Section 5.2.

- [ ] **`start_heartbeat_timer()`** -- Start heartbeat timer (leader only).
  - Call `heartbeat_timer_->reset()` with callback that enqueues `HeartbeatTimeoutEvent`.
  - **Paper**: Section 5.2 (heartbeat mechanism).

- [ ] **`stop_heartbeat_timer()`** -- Stop heartbeat timer.
  - Call `heartbeat_timer_->stop()`.

---

### RaftLog (`src/core/raft_log.cpp`)

- [ ] **Constructor** -- Load entries from LogStore into cache.
  - Query `store_->first_index()` and `store_->last_index()`.
  - Load all existing entries into `cache_`.
  - **Paper**: N/A (implementation design).

- [ ] **`last_index()`** -- Return last entry index.
  - Check cache, then snapshot metadata, then return `LogIndex::none()`.

- [ ] **`last_term()`** -- Return term of last entry.
  - Check cache, then snapshot metadata, then return `Term{0}`.

- [ ] **`entry_at(LogIndex)`** -- Retrieve entry at given index.
  - Compute offset from first cached index, return from cache.

- [ ] **`term_at(LogIndex)`** -- Return term at given index.
  - Handle sentinel (index 0), snapshot boundary, and cache lookup.
  - **Paper**: Section 5.3 (log matching check).

- [ ] **`slice(LogIndex, LogIndex)`** -- Return entries in range.
  - Bounds check, compute cache offsets, return sub-vector.

- [x] **`match_term(LogIndex, Term)`** -- Log matching check.
  - Already implemented (delegates to `term_at()`).
  - **Paper**: Section 5.3.

- [ ] **`append(LogEntry)`** -- Append single entry.
  - Assign index, persist via store, add to cache.

- [ ] **`append_entries(vector<LogEntry>)`** -- Append multiple entries.
  - Assign indexes, persist via store, add to cache.

- [ ] **`truncate_from(LogIndex)`** -- Remove entries from index onward.
  - Persist via store, remove from cache.
  - **Paper**: Section 5.3 (handling conflicting entries).

- [ ] **`compact(LogIndex, Term)`** -- Discard entries covered by snapshot.
  - Update snapshot metadata, remove from cache and store.
  - **Paper**: Section 7.

- [ ] **`apply_snapshot(LogIndex, Term)`** -- Handle incoming snapshot.
  - Update snapshot metadata, keep entries after snapshot or clear all.
  - **Paper**: Section 7.

---

### ClusterMembership (`src/core/cluster_membership.cpp`)

- [ ] **`has_quorum(set<NodeId>)`** -- Check quorum.
  - Stable: majority of current config.
  - Joint: separate majorities of both C_old and C_new.
  - **Paper**: Section 6 (joint consensus).

- [ ] **`begin_change(ClusterConfig)`** -- Start membership change.
  - Transition Stable -> Joint.
  - **Paper**: Section 6.

- [ ] **`finalize_change()`** -- Complete membership change.
  - Transition Joint -> Stable(C_new).
  - **Paper**: Section 6.

- [ ] **`abort_change()`** -- Abort membership change.
  - Revert to C_old.
  - **Paper**: Section 6.

- [~] **`all_voting_members()`** -- Return union of all members.
  - Currently returns only `current_.members`. Need to include `new_` in Joint phase.
  - **Paper**: Section 6.

- [ ] **`serialize()`** -- Serialize membership state to bytes.
  - Include phase, current members, new members (if Joint).

- [ ] **`deserialize(span<uint8_t>)`** -- Deserialize membership state.

---

## Storage Module

### FileLogStore (`src/storage/file_log_store.cpp`)

- [ ] **Constructor** -- Create directory, open/create data and index files, rebuild index if needed.
- [ ] **`first_index()`** -- Return first index in index map.
- [ ] **`last_index()`** -- Return last index in index map.
- [ ] **`entry_at(LogIndex)`** -- Seek to byte offset, deserialize entry.
- [ ] **`entries(LogIndex, LogIndex)`** -- Read range of entries.
- [ ] **`append(vector<LogEntry>)`** -- Serialize and append entries, update index, fsync.
  - **Critical**: Must fsync before returning (Section 5.3 safety).
- [ ] **`truncate_from(LogIndex)`** -- Truncate data file, rewrite index file, fsync.
- [ ] **`truncate_before(LogIndex)`** -- Remove early entries (for log compaction).
- [ ] **`sync()`** -- fsync both data and index files.

### FilePersistentState (`src/storage/file_persistent_state.cpp`)

- [ ] **Constructor** -- Create directory, recovery logic (state.json or state.json.tmp).
- [ ] **`save(Term, optional<NodeId>)`** -- Atomic persist.
  - Write JSON to state.json.tmp.
  - fsync file.
  - Rename to state.json.
  - fsync directory.
  - **Critical**: Must be durable before any RPC reply (Section 5.2, Figure 2).

### FileSnapshotStore (`src/storage/file_snapshot_store.cpp`)

- [ ] **Constructor** -- Create directory, clean up tmp files, load cached metadata.
- [ ] **`latest_metadata()`** -- Return cached metadata.
- [ ] **`read_snapshot()`** -- Read current.data into memory.
- [ ] **`read_chunk(offset, length)`** -- Read chunk from current.data.
- [ ] **`snapshot_size()`** -- Return file size of current.data.
- [ ] **`begin_snapshot(SnapshotMetadata)`** -- Write tmp.meta, open tmp.data.
- [ ] **`write_chunk(offset, span<uint8_t>)`** -- Write to tmp.data at offset.
- [ ] **`finalize_snapshot()`** -- fsync, rename tmp -> current, update cache.
  - **Paper**: Section 7 (atomic snapshot installation).
- [ ] **`abort_snapshot()`** -- Delete tmp files.

---

## Transport Module

### TcpTransport (`src/rpc/tcp_transport.cpp`)

- [ ] **`start(NodeId, PeerAddress, MessageHandler)`** -- Bind, listen, start threads.
  - Create TCP socket with SO_REUSEADDR.
  - Bind to address, listen with backlog.
  - Start acceptor thread (accept loop).
  - Start I/O thread (poll loop).
  - Set `running_ = true`.

- [ ] **`stop()`** -- Close listening socket, join threads, close connections.

- [ ] **`add_peer(NodeId, PeerAddress)`** -- Initiate outgoing connection with backoff.

- [ ] **`remove_peer(NodeId)`** -- Close connection, remove from map.

- [ ] **`send(NodeId, MessageType, vector<uint8_t>)`** -- Frame and queue message.
  - 4-byte big-endian length + 1-byte type + payload.

- [ ] **`broadcast(MessageType, vector<uint8_t>)`** -- Send to all peers.

- [ ] **`accept_loop()`** (private) -- Accept incoming connections, read handshake.

- [ ] **`io_loop()`** (private) -- poll() multiplexing, read/write handling.

- [ ] **`handle_readable(Connection&)`** (private) -- Extract framed messages from recv buffer.

- [ ] **`handle_writable(Connection&)`** (private) -- Flush send queue.

- [ ] **`try_reconnect(Connection&)`** (private) -- Reconnect with exponential backoff.

- [ ] **`frame_message()`** (private) -- Build framed message bytes.

- [ ] **`extract_message()`** (private) -- Parse one complete message from recv buffer.

---

## Serialization Module

### JsonSerializer (`src/util/serializer.cpp`)

#### Serialize (8 methods)

- [ ] **`serialize(RequestVote)`** -- JSON: type, term, candidate_id, last_log_index, last_log_term.
- [ ] **`serialize(RequestVoteReply)`** -- JSON: type, term, vote_granted.
- [ ] **`serialize(AppendEntries)`** -- JSON: type, term, leader_id, prev_log_index, prev_log_term, entries (nested array), leader_commit.
- [ ] **`serialize(AppendEntriesReply)`** -- JSON: type, term, success, conflict_term, conflict_index.
- [ ] **`serialize(InstallSnapshot)`** -- JSON: type, term, leader_id, last_included_index, last_included_term, offset, data (base64), done.
- [ ] **`serialize(InstallSnapshotReply)`** -- JSON: type, term.
- [ ] **`serialize(ClientRequest)`** -- JSON: type, client_id, sequence_number, command (base64).
- [ ] **`serialize(ClientResponse)`** -- JSON: type, success, error, leader_hint, response (base64).

#### Deserialize (9 methods)

- [ ] **`detect_type(span<uint8_t>)`** -- Parse JSON "type" field, return MessageType.
- [ ] **`deserialize_request_vote()`**
- [ ] **`deserialize_request_vote_reply()`**
- [ ] **`deserialize_append_entries()`**
- [ ] **`deserialize_append_entries_reply()`**
- [ ] **`deserialize_install_snapshot()`**
- [ ] **`deserialize_install_snapshot_reply()`**
- [ ] **`deserialize_client_request()`**
- [ ] **`deserialize_client_response()`**

#### Log Entry and Snapshot Metadata (4 methods)

- [ ] **`serialize_entry(LogEntry)`** -- JSON: term, index, type, data (base64).
- [ ] **`deserialize_entry(span<uint8_t>)`**
- [ ] **`serialize_snapshot_meta(SnapshotMetadata)`** -- JSON: last_included_index, last_included_term, configuration (base64).
- [ ] **`deserialize_snapshot_meta(span<uint8_t>)`**

---

## Utilities Module

### RealTimer (`src/util/timer.cpp`)

- [ ] **`reset(Duration, Callback)`** -- Launch or restart timer thread.
  - Store callback and compute deadline.
  - Launch jthread if not running.
  - Wake thread via condition variable.

- [ ] **`stop()`** -- Cancel timer.
  - Set `active_ = false`, `shutdown_ = true`.
  - Notify and join thread.

- [ ] **`running()`** -- Return `active_` under mutex.

---

## Tests

### Unit Tests (`tests/unit/`)

- [ ] Create `tests/unit/CMakeLists.txt` with test targets.
- [ ] **`test_raft_log.cpp`** -- RaftLog: append, truncate, snapshot, term lookups.
- [ ] **`test_cluster_membership.cpp`** -- Quorum checks, joint consensus.
- [ ] **`test_serializer.cpp`** -- Round-trip for every message type.
- [ ] **`test_timer.cpp`** -- Timer behavior with test doubles.
- [ ] **`test_persistent_state.cpp`** -- Atomic save, recovery, crash simulation.
- [ ] **`test_log_store.cpp`** -- File I/O, index rebuild, fsync.
- [ ] **`test_snapshot_store.cpp`** -- Chunked write/read, finalization.

### Integration Tests (`tests/integration/`)

Test files are listed in `CMakeLists.txt` but the source files need to be created:

- [ ] **`test_leader_election.cpp`** -- Election in 3-node cluster, quorum.
- [ ] **`test_log_replication.cpp`** -- Entry replication to majority.
- [ ] **`test_follower_catchup.cpp`** -- Slow follower catches up.
- [ ] **`test_leader_failover.cpp`** -- Leader death, new election.
- [ ] **`test_stale_term.cpp`** -- Stale leader stepped down.
- [ ] **`test_snapshot_install.cpp`** -- Follower receives snapshot.
- [ ] **`test_network_partition.cpp`** -- Minority cannot elect leader.
- [ ] **`test_restart_recovery.cpp`** -- Node restarts, recovers state.
- [ ] **`test_membership_change.cpp`** -- Joint consensus add/remove node.
- [ ] **`test_client_interaction.cpp`** -- Client submit, redirect, exactly-once.

### Test Infrastructure (`tests/common/`)

- [ ] **Mock Transport** -- In-process message passing for integration tests.
- [ ] **Mock StateMachine** -- Simple counter or KV store for testing.
- [ ] **Mock TimerFactory** -- Deterministic timers that can be manually advanced.
- [ ] **Mock LogStore / PersistentState / SnapshotStore** -- In-memory implementations.
- [ ] **Test cluster harness** -- Helper to spin up N RaftNodes and control them.

---

## Examples

### Example Binaries (`examples/`)

- [ ] **`examples/CMakeLists.txt`** -- Build configuration for example binaries.
- [ ] **`raft_node_main.cpp`** -- Main binary that starts a single Raft node with command-line arguments for node ID, peers, and data directory.
- [ ] **`raft_client_main.cpp`** -- Client binary that connects to a Raft node, sends commands, and displays responses.
- [ ] **`kv_state_machine.h`** -- Example key-value store StateMachine implementation.

---

## Summary

| Module           | Total TODOs | Key Paper Sections        |
|------------------|-------------|---------------------------|
| Core (RaftNode)  | 25          | 5.1, 5.2, 5.3, 5.4, 7, 8 |
| Core (RaftLog)   | 11          | 5.3, 7                    |
| Core (Membership)| 7           | 6                         |
| Storage          | 16          | 5.3 (durability), 7       |
| Transport        | 12          | N/A (infrastructure)      |
| Serialization    | 22          | N/A (infrastructure)      |
| Utilities (Timer)| 3           | 5.2                       |
| Tests            | 21          | N/A                       |
| Examples         | 4           | N/A                       |
| **Total**        | **121**     |                           |
