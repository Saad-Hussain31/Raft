# Architecture

This document provides a deep dive into the architecture of the Raft consensus implementation.

## Component Diagram

```
+===========================================================================+
|                              RaftNode                                      |
|                        (src/core/raft_node.cpp)                            |
|                                                                            |
|  +-------------------+     +-------------------+     +------------------+  |
|  | Role Management   |     | Event Loop        |     | Client Interface |  |
|  | - become_follower |     | - run_event_loop  |     | - submit_command |  |
|  | - start_election  |     | - enqueue         |     | - pending_reqs   |  |
|  | - become_leader   |     | - dispatch_msg    |     | - client_sessions|  |
|  +-------------------+     +-------------------+     +------------------+  |
|                                     |                                      |
|  +-------------------+     +--------+--------+     +--------------------+  |
|  | RPC Handlers      |     | Log Operations  |     | Snapshot Logic     |  |
|  | - RequestVote     |     | - advance_commit|     | - maybe_take_snap  |  |
|  | - AppendEntries   |     | - apply_entries |     | - install_snapshot |  |
|  | - InstallSnapshot |     | - send_append   |     | - send_snapshot    |  |
|  +-------------------+     +-----------------+     +--------------------+  |
|                                                                            |
+====+===========+===========+============+=============+=========+=========+
     |           |           |            |             |         |
     v           v           v            v             v         v
+---------+ +---------+ +---------+ +----------+ +---------+ +--------+
|Transport| |RaftLog  | |Persist- | |Snapshot  | |Serial-  | |Timer   |
|  (TCP)  | |(cache)  | |entState | |Store     | |izer     | |Factory |
+---------+ +----+----+ +---------+ +----------+ +---------+ +--------+
                 |
                 v
            +---------+
            |LogStore |
            | (file)  |
            +---------+
```

## Component Responsibilities

### RaftNode (`include/raft/core/raft_node.h`)

The RaftNode is the central orchestrator. It:

- Owns the event loop thread and the event queue.
- Holds references (via `std::unique_ptr`) to every injected component.
- Manages all Raft state: role, current term, voted-for, commit index, last applied.
- Coordinates role transitions (Follower, Candidate, Leader).
- Dispatches incoming RPCs to the appropriate handler.
- Drives log replication and commit advancement.
- Manages pending client requests and session tracking.

The RaftNode does **not** directly perform I/O. It delegates to the Transport (for networking), LogStore/PersistentState/SnapshotStore (for disk), and Serializer (for encoding/decoding).

### RaftLog (`include/raft/core/raft_log.h`)

The RaftLog is an in-memory cache layered on top of the persistent LogStore. It:

- Maintains a `std::vector<LogEntry>` cache of all entries after the last snapshot.
- Translates between absolute log indexes and physical storage offsets.
- Provides O(1) access to entries by index (via cache offset arithmetic).
- Handles snapshot compaction: when a snapshot is taken, entries before `last_included_index` are purged from both the cache and the store.
- Delegates durable writes to the LogStore.

All access to RaftLog is from the single event-loop thread, so no internal locking is needed.

### StateMachine (`include/raft/core/state_machine.h`)

The StateMachine is the application-level component that gives meaning to replicated commands. It is an abstract interface with three operations:

- `apply(index, term, type, data)` -- Apply a committed entry. Must be deterministic.
- `take_snapshot()` -- Capture the full state for log compaction.
- `restore_snapshot(data)` -- Replace state from a snapshot.

The Raft library treats command data as opaque byte vectors. Only the StateMachine interprets them.

### ClusterMembership (`include/raft/core/cluster_membership.h`)

Tracks the current cluster configuration and manages joint-consensus membership changes:

- **Stable phase**: A single configuration. Quorum = majority of members.
- **Joint phase**: Two configurations (C_old, C_new). Quorum requires separate majorities of both.

The `has_quorum()` method encapsulates all quorum logic, making the rest of the system agnostic to whether a membership change is in progress.

### Transport (`include/raft/rpc/transport.h`)

The Transport handles all network communication. The concrete `TcpTransport` implementation:

- Uses POSIX TCP sockets with non-blocking I/O.
- Runs an acceptor thread for incoming connections.
- Runs an I/O thread using `poll()` for multiplexed read/write.
- Frames messages with a 4-byte big-endian length prefix.
- Reconnects to peers with exponential backoff on failure.
- Invokes a `MessageHandler` callback (which posts to the event queue) for each received message.

The Transport is completely decoupled from the consensus logic. It never deserializes message payloads.

### Storage Layer

Three interfaces compose the storage layer:

**LogStore** (`include/raft/storage/log_store.h`):
- Append-only file with a companion index file.
- `append()` writes entries and fsyncs before returning.
- `truncate_from()` handles log conflict resolution.
- `truncate_before()` handles log compaction after snapshots.

**PersistentState** (`include/raft/storage/persistent_state.h`):
- Single JSON file storing `currentTerm` and `votedFor`.
- Atomic updates via write-temp, fsync, rename.
- Recovery logic handles incomplete renames.

**SnapshotStore** (`include/raft/storage/snapshot_store.h`):
- Stores snapshot data and metadata as separate files.
- Supports chunked write for streaming from leader.
- Atomic finalization via temp-file rename.
- Cleanup of incomplete snapshots at startup.

### Serializer (`include/raft/util/serializer.h`)

Converts RPC message structs and log entries to/from byte vectors. The default implementation uses nlohmann/json for human-readable wire format. The interface allows swapping in binary formats (Protobuf, FlatBuffers) without touching the consensus core.

### Timer (`include/raft/util/timer.h`)

Provides election and heartbeat timers. The `TimerFactory` abstraction allows tests to inject deterministic timers. The default `RealTimer` implementation uses `std::jthread` and `std::condition_variable` with wall-clock deadlines.

### Logger and Metrics (`include/raft/util/logger.h`, `include/raft/util/metrics.h`)

Cross-cutting concerns injected into the RaftNode:

- **Logger**: Thread-safe structured logging backed by spdlog. Each node gets a distinct logger name.
- **Metrics**: Thread-safe counters and gauges. Default implementation uses mutex-protected maps.

## Thread Model

The implementation uses a **single-threaded event loop** for all consensus logic, with background threads only for I/O and timers.

```
Thread Architecture:

  +----------------------------------------------+
  |  Event Loop Thread (run_event_loop)           |
  |  - Processes all events serially              |
  |  - No fine-grained locking needed             |
  |  - All consensus state mutated here           |
  +---+------------------+-----------+------------+
      ^                  ^           ^
      |                  |           |
   enqueue()          enqueue()   enqueue()
      |                  |           |
  +---+----+      +------+-----+   +------+------+
  |Transport|      |Election    |   |Heartbeat    |
  |I/O      |      |Timer       |   |Timer        |
  |Threads  |      |Thread      |   |Thread       |
  +---------+      +------------+   +-------------+
  (2 threads:       (1 jthread)      (1 jthread)
   acceptor +
   I/O loop)
```

### Thread Inventory

| Thread              | Count | Purpose                                                |
|---------------------|-------|--------------------------------------------------------|
| Event loop          | 1     | Processes events, runs consensus logic                 |
| Transport acceptor  | 1     | Accepts incoming TCP connections                       |
| Transport I/O       | 1     | Multiplexed read/write via poll()                      |
| Election timer      | 1     | Fires election timeout events                          |
| Heartbeat timer     | 1     | Fires heartbeat timeout events                         |
| **Total per node**  | **5** |                                                        |

### Synchronization

The only shared mutable state is the **event queue**:

```cpp
std::mutex queue_mutex_;
std::condition_variable queue_cv_;
std::queue<Event> event_queue_;
```

All other state (role, term, log, peer states, pending requests) is accessed exclusively from the event loop thread.

The Transport has its own internal mutex (`connections_mutex_`) for managing the connection map, and each Connection has a `send_mutex` for its outgoing message queue. These are internal to the Transport and invisible to the consensus logic.

## Event Loop Design

The event loop follows a classic **reactor pattern**:

```
loop:
    lock(queue_mutex_)
    wait(queue_cv_, !event_queue_.empty())
    event = event_queue_.pop()
    unlock(queue_mutex_)

    match event:
        IncomingMessage  -> dispatch_message(msg)
        ElectionTimeout  -> start_election()
        HeartbeatTimeout -> send_append_entries_to_all()
        ShutdownEvent    -> break

    apply_committed_entries()
    maybe_take_snapshot()
```

### Event Types

The `Event` variant is defined as:

```cpp
using Event = std::variant<
    IncomingMessage,         // RPC received from a peer
    ElectionTimeoutEvent,    // Election timer fired
    HeartbeatTimeoutEvent,   // Heartbeat timer fired (leader only)
    ShutdownEvent            // Graceful shutdown requested
>;
```

### Event Sources

1. **Transport**: When a complete framed message is received, the Transport calls the `MessageHandler`, which calls `enqueue(IncomingMessage{...})`.
2. **Election Timer**: When the timer expires, its callback calls `enqueue(ElectionTimeoutEvent{})`.
3. **Heartbeat Timer**: When the timer expires, its callback calls `enqueue(HeartbeatTimeoutEvent{})`.
4. **stop()**: Posts a `ShutdownEvent` to cleanly exit the loop.

## RPC Handler Interactions

### RequestVote Flow

```
                     Candidate                        Voter
                        |                               |
                        |--- RequestVote(term, ...) --->|
                        |                               |
                        |                     maybe_step_down(term)
                        |                     check votedFor
                        |                     check log up-to-date
                        |                     persist votedFor
                        |                     reset election timer
                        |                               |
                        |<-- RequestVoteReply(granted) --|
                        |                               |
                  add to votes_received_
                  if has_quorum() -> become_leader()
```

### AppendEntries Flow

```
                     Leader                           Follower
                        |                               |
                        |--- AppendEntries(entries) ---->|
                        |                               |
                        |                     maybe_step_down(term)
                        |                     check prevLogIndex/Term
                        |                     truncate conflicting
                        |                     append new entries
                        |                     persist to LogStore
                        |                     update commitIndex
                        |                               |
                        |<-- AppendEntriesReply(ok) ----|
                        |                               |
                  update matchIndex[follower]
                  update nextIndex[follower]
                  advance_commit_index()
                  apply_committed_entries()
```

### InstallSnapshot Flow

```
                     Leader                           Follower
                        |                               |
                        |--- InstallSnapshot(chunk) --->|
                        |     (offset=0, done=false)    |
                        |                     begin_snapshot()
                        |                     write_chunk(0, data)
                        |<-- Reply ----------------------|
                        |                               |
                        |--- InstallSnapshot(chunk) --->|
                        |     (offset=N, done=true)     |
                        |                     write_chunk(N, data)
                        |                     finalize_snapshot()
                        |                     restore state machine
                        |                     update log
                        |<-- Reply ----------------------|
                        |                               |
                  update nextIndex, matchIndex
                  continue with AppendEntries
```

### Client Command Flow

```
  Client                  Leader                       Followers
    |                       |                              |
    |-- submit_command() -->|                              |
    |                       |                              |
    |              append to log                           |
    |              store pending callback                  |
    |                       |                              |
    |                       |--- AppendEntries ----------->|
    |                       |<-- Reply (success) ----------|
    |                       |                              |
    |              advance_commit_index()                  |
    |              apply_committed_entries()                |
    |              invoke callback(result)                  |
    |                       |                              |
    |<-- ClientResponse ----|                              |
```

## Startup and Recovery Sequence

When `RaftNode::start()` is called:

```
1. Recover persistent state
   ├── Read currentTerm and votedFor from PersistentState
   └── Cache in current_term_ and voted_for_

2. Recover log
   ├── RaftLog constructor loads entries from LogStore
   ├── Check SnapshotStore for existing snapshot
   │   ├── If snapshot exists, restore state machine
   │   └── Set last_applied_ = snapshot.last_included_index
   └── Apply any committed entries after the snapshot

3. Initialize cluster membership
   ├── Build ClusterConfig from config_.initial_peers
   └── Override with latest Config entry in log (if any)

4. Start transport
   ├── transport_->start(local_id, address, handler)
   └── Add all peers to transport

5. Create and start timers
   ├── election_timer_ = timer_factory_->create()
   ├── heartbeat_timer_ = timer_factory_->create()
   └── Reset election timer (start as Follower)

6. Launch event loop thread
   └── Set running_ = true
```

## Shutdown Sequence

When `RaftNode::stop()` is called:

```
1. Set running_ = false
2. Post ShutdownEvent to event queue
3. Join event loop thread
4. Stop election timer
5. Stop heartbeat timer
6. Stop transport
7. Fail pending client requests with ShuttingDown
```

## Memory Layout

Each RaftNode instance holds:

- **Configuration** (immutable, ~200 bytes + peer list)
- **10 unique_ptrs** to injected components (~80 bytes of pointers)
- **RaftLog** with cache vector (variable, grows with uncommitted entries)
- **Volatile state** (role, term, commit/applied indexes, leader_id -- ~50 bytes)
- **Peer states map** (leader only, ~64 bytes per peer)
- **Votes received set** (candidate only, ~64 bytes per vote)
- **ClusterMembership** (~128 bytes + member sets)
- **Pending client requests map** (leader only, variable)
- **Client sessions map** (leader only, variable)
- **Event queue** (variable, typically small)

The dominant memory consumer is the RaftLog cache, which holds all entries between the last snapshot and the current log head. Snapshot threshold configuration controls this size.
