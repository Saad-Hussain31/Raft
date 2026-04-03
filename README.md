# Raft Consensus Implementation

A production-grade scaffold for the [Raft consensus algorithm](https://raft.github.io/raft.pdf) in modern C++20. Every interface is fully specified, every stub is annotated with exactly what must happen, and the architecture is shaped for real-world deployment -- but the core logic is left for the implementer to fill in.

## Table of Contents

- [Project Overview](#project-overview)
- [Raft Protocol Summary](#raft-protocol-summary)
- [Design Goals](#design-goals)
- [Architecture Overview](#architecture-overview)
- [Directory Structure](#directory-structure)
- [Third-Party Dependencies](#third-party-dependencies)
- [Build Instructions](#build-instructions)
- [Configuration](#configuration)
- [Module Interaction and Data Flow](#module-interaction-and-data-flow)
- [Logging and Metrics](#logging-and-metrics)
- [Testing Strategy](#testing-strategy)
- [Code Coverage](#code-coverage)
- [Snapshot and Persistence](#snapshot-and-persistence)
- [Safety Notes and Protocol Assumptions](#safety-notes-and-protocol-assumptions)
- [Extension Points](#extension-points)
- [Running a Local Cluster](#running-a-local-cluster)
- [License](#license)

---

## Project Overview

This project provides a complete, production-shaped implementation scaffold for the Raft consensus protocol. It is written in C++20 with strict compiler warnings, sanitizer support, and coverage instrumentation. The codebase is organized into clearly separated modules -- core consensus, storage, transport, serialization, and utilities -- each defined by an abstract interface with a concrete file-backed or TCP-backed default implementation.

All method bodies contain detailed TODO comments that describe exactly what must be implemented, which invariants must hold, and which sections of the Raft paper apply. The header files carry extensive Doxygen documentation covering the contract, thread safety, and edge cases for every public method.

## Raft Protocol Summary

Raft is a consensus algorithm for managing a replicated log across a cluster of servers. It guarantees that all servers agree on the same sequence of commands, even in the presence of failures, as long as a majority of servers are operational.

The protocol decomposes into three sub-problems:

1. **Leader Election** -- Exactly one leader is elected per term using randomized timeouts and majority voting.
2. **Log Replication** -- The leader accepts client commands, appends them to its log, and replicates them to followers via AppendEntries RPCs.
3. **Safety** -- A set of invariants (Election Safety, Leader Append-Only, Log Matching, Leader Completeness, State Machine Safety) guarantee that committed entries are never lost or reordered.

Additional mechanisms include **log compaction** via snapshots (InstallSnapshot RPC), **cluster membership changes** via joint consensus, and **client interaction** with exactly-once semantics through session tracking.

## Design Goals

- **Closely follows the paper.** Every RPC, every state transition, and every invariant maps directly to a section in [In Search of an Understandable Consensus Algorithm (Extended Version)](https://raft.github.io/raft.pdf). Paper section references are included in code comments.
- **Production-shaped.** The architecture uses dependency injection, abstract interfaces, and a single-threaded event loop -- patterns found in real Raft implementations (etcd/raft, HashiCorp Raft, Scylladb Raft). The scaffold is not a toy.
- **Extensible.** Every major component is behind an interface. Swap the transport (TCP for gRPC), the storage backend (files for RocksDB), the serializer (JSON for Protobuf), or the state machine (KV store for lock manager) without touching the consensus core.
- **Testable.** Interfaces allow mock injection. The event-loop architecture makes behavior deterministic under test harnesses. Timer and transport can be replaced with test doubles.
- **Safe by default.** Strong types prevent mixing terms, indexes, and node IDs at compile time. Persistent state uses atomic write-rename-fsync. The Result type replaces exceptions for predictable error handling.

## Architecture Overview

The system is composed of the following major components:

| Component            | Header                              | Description                                                      |
|----------------------|-------------------------------------|------------------------------------------------------------------|
| **RaftNode**         | `include/raft/core/raft_node.h`     | Central consensus module. Runs the event loop, manages role transitions, coordinates all other components. |
| **RaftLog**          | `include/raft/core/raft_log.h`      | In-memory log cache layered on top of LogStore. Handles snapshot offsets and index translation. |
| **StateMachine**     | `include/raft/core/state_machine.h` | Abstract application state machine. Receives committed entries and produces snapshots. |
| **ClusterMembership**| `include/raft/core/cluster_membership.h` | Tracks current and transitional (joint consensus) cluster configurations. |
| **Configuration**    | `include/raft/core/configuration.h` | Immutable startup configuration: identity, peers, timing, paths, thresholds. |
| **Transport**        | `include/raft/rpc/transport.h`      | Abstract network layer. Default: TCP with non-blocking I/O and poll(). |
| **Messages**         | `include/raft/rpc/messages.h`       | All RPC message structs: RequestVote, AppendEntries, InstallSnapshot, Client RPCs. |
| **LogStore**         | `include/raft/storage/log_store.h`  | Abstract persistent log storage. Default: append-only file with index map. |
| **PersistentState**  | `include/raft/storage/persistent_state.h` | Durable currentTerm + votedFor. Default: JSON file with atomic write-rename. |
| **SnapshotStore**    | `include/raft/storage/snapshot_store.h`   | Durable snapshot storage with chunked read/write. Default: file-backed. |
| **Serializer**       | `include/raft/util/serializer.h`    | Message and entry serialization. Default: JSON via nlohmann/json. |
| **Logger**           | `include/raft/util/logger.h`        | Abstract structured logging. Default: spdlog with per-node prefixed output. |
| **Metrics**          | `include/raft/util/metrics.h`       | Counters and gauges for observability. Default: in-memory atomics. |
| **Timer**            | `include/raft/util/timer.h`         | Election and heartbeat timers. Default: jthread + condition_variable. |
| **Types**            | `include/raft/util/types.h`         | Strong types (Term, LogIndex, NodeId), LogEntry, SnapshotMetadata, Result, ErrorCode. |

## Directory Structure

```
Raft/
├── CMakeLists.txt                          # Top-level build configuration
├── README.md                               # This file
├── .clang-format                           # Code formatting rules
├── .gitignore
├── .gitmodules                             # Submodule references
│
├── cmake/
│   ├── CompilerWarnings.cmake              # Strict -Wall -Wextra -Wpedantic etc.
│   ├── Coverage.cmake                      # --coverage flags (lcov)
│   └── Sanitizers.cmake                    # ASan + UBSan support
│
├── include/raft/
│   ├── raft.h                              # Umbrella header
│   ├── core/
│   │   ├── cluster_membership.h
│   │   ├── configuration.h
│   │   ├── raft_log.h
│   │   ├── raft_node.h                     # Central consensus module
│   │   └── state_machine.h
│   ├── rpc/
│   │   ├── messages.h                      # All RPC message definitions
│   │   └── transport.h                     # Network transport interface
│   ├── storage/
│   │   ├── log_store.h
│   │   ├── persistent_state.h
│   │   └── snapshot_store.h
│   └── util/
│       ├── logger.h
│       ├── metrics.h
│       ├── serializer.h
│       ├── timer.h
│       └── types.h                         # Strong types, Result, ErrorCode
│
├── src/
│   ├── CMakeLists.txt                      # Builds raft_lib static library
│   ├── core/
│   │   ├── cluster_membership.cpp
│   │   ├── raft_log.cpp
│   │   └── raft_node.cpp                   # Stub implementations with TODOs
│   ├── rpc/
│   │   └── tcp_transport.cpp               # TCP transport stub
│   ├── storage/
│   │   ├── file_log_store.cpp
│   │   ├── file_persistent_state.cpp
│   │   └── file_snapshot_store.cpp
│   └── util/
│       ├── logger.cpp                      # spdlog-backed (implemented)
│       ├── metrics.cpp                     # In-memory atomics (implemented)
│       ├── serializer.cpp                  # JSON serializer stub
│       └── timer.cpp                       # Wall-clock timer stub
│
├── tests/
│   ├── CMakeLists.txt
│   ├── common/                             # Shared test utilities and mocks
│   ├── unit/                               # Per-module unit tests
│   └── integration/
│       └── CMakeLists.txt                  # Integration test suite
│
├── examples/                               # Example node and client binaries
│
├── docs/
│   ├── architecture.md                     # Deep-dive architecture document
│   ├── protocol.md                         # Protocol specification
│   ├── implementation_checklist.md         # TODO tracker by module
│   └── local_runtime_guide.md             # Running a local cluster
│
├── scripts/
│   ├── launch_cluster.sh                   # Launch N nodes on localhost
│   ├── launch_client.sh                    # Launch a client
│   └── run_coverage.sh                     # Build + test + lcov report
│
└── third_party/
    ├── README.md                           # Submodule initialization guide
    ├── googletest/                         # Google Test framework
    ├── spdlog/                             # Structured logging library
    └── json/                               # nlohmann/json
```

## Third-Party Dependencies

All dependencies are included as Git submodules under `third_party/`.

| Library        | Purpose                  | URL                                          |
|----------------|--------------------------|----------------------------------------------|
| Google Test    | Unit and integration testing | https://github.com/google/googletest.git |
| spdlog         | Structured logging       | https://github.com/gabime/spdlog.git         |
| nlohmann/json  | JSON serialization       | https://github.com/nlohmann/json.git         |

### Initializing Submodules

After cloning, initialize and update the submodules:

```bash
git submodule update --init --recursive
```

If you cloned without `--recurse-submodules`, this step is required before building.

## Build Instructions

### Prerequisites

- C++20-capable compiler (GCC 12+, Clang 14+)
- CMake 3.22+
- lcov (optional, for coverage reports)
- A POSIX system (Linux, macOS)

### Basic Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
```

### Build Options

| CMake Option              | Default | Description                                |
|---------------------------|---------|--------------------------------------------|
| `RAFT_BUILD_TESTS`       | `ON`    | Build unit and integration tests           |
| `RAFT_BUILD_EXAMPLES`    | `ON`    | Build example applications                 |
| `RAFT_ENABLE_COVERAGE`   | `OFF`   | Enable gcov/lcov coverage instrumentation  |
| `RAFT_ENABLE_SANITIZERS` | `OFF`   | Enable AddressSanitizer + UBSan            |

### Build with Tests and Sanitizers

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DRAFT_BUILD_TESTS=ON \
         -DRAFT_ENABLE_SANITIZERS=ON
cmake --build . -j$(nproc)
ctest --test-dir . --output-on-failure
```

### Build with Coverage

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DRAFT_ENABLE_COVERAGE=ON
cmake --build . -j$(nproc)
ctest --test-dir . --output-on-failure
# Generate the report:
../scripts/run_coverage.sh
```

### Running Tests

```bash
cd build
ctest --output-on-failure
# Or run directly:
./tests/integration/raft_integration_tests
```

## Configuration

The `Configuration` struct (in `include/raft/core/configuration.h`) holds all tuneable parameters. Below is an example for a 3-node local cluster:

```cpp
raft::Configuration config;

// Identity
config.node_id       = raft::NodeId{1};
config.cluster_id    = raft::ClusterId{"my-raft-cluster"};
config.listen_address = raft::PeerAddress{"127.0.0.1", 9001};

// Cluster peers (include self)
config.initial_peers = {
    {raft::NodeId{1}, {"127.0.0.1", 9001}},
    {raft::NodeId{2}, {"127.0.0.1", 9002}},
    {raft::NodeId{3}, {"127.0.0.1", 9003}},
};

// Timing (milliseconds)
config.timers.election_timeout_min = std::chrono::milliseconds{150};
config.timers.election_timeout_max = std::chrono::milliseconds{300};
config.timers.heartbeat_interval   = std::chrono::milliseconds{50};

// Storage
config.data_dir = "/tmp/raft_node_1";

// Snapshotting
config.snapshot_threshold  = 10000;   // entries between snapshots
config.snapshot_chunk_size = 1048576; // 1 MiB chunks for InstallSnapshot

// Networking
config.max_inflight_rpcs = 1;
config.rpc_timeout       = std::chrono::milliseconds{100};

// Logging
config.log_level = raft::LogLevel::Debug;
```

## Module Interaction and Data Flow

```
  Client
    |
    | submit_command()
    v
+-----------+    enqueue()    +----------------+
| Any Thread | ------------> | Event Queue    |
+-----------+                | (mutex + cv)   |
                             +-------+--------+
                                     |
                                     v
                           +---------+---------+
                           |  Event Loop       |  <-- single thread
                           |  (RaftNode::      |
                           |   run_event_loop) |
                           +---------+---------+
                                     |
              +----------+-----------+-----------+----------+
              |          |                       |          |
              v          v                       v          v
        +-----------+  +--------+          +---------+  +--------+
        | Election  |  | Append |          | Install |  | Client |
        | Timeout   |  | Entry  |          | Snapshot|  | Request|
        | Handler   |  | Handler|          | Handler |  | Handler|
        +-----------+  +--------+          +---------+  +--------+
              |          |                       |          |
              +----+-----+-----+-----------+----+----------+
                   |           |           |
                   v           v           v
             +---------+  +--------+  +------------+
             | RaftLog |  | Persis-|  | Snapshot   |
             | (cache) |  | tent   |  | Store      |
             |         |  | State  |  |            |
             +----+----+  +---+----+  +-----+------+
                  |            |              |
                  v            v              v
             +---------+  +--------+  +------------+
             | LogStore|  | JSON   |  | File       |
             | (file)  |  | file   |  | storage    |
             +---------+  +--------+  +------------+

                          +------------+
                          | Transport  |
                          | (TCP)      |  <-- I/O threads
                          +-----+------+
                                |
                       send() / receive()
                                |
                          +-----+------+
                          |  Network   |
                          +------------+
```

### Data Flow Summary

1. **Incoming RPCs**: Transport I/O threads receive bytes, frame messages, and post `IncomingMessage` events to the event queue.
2. **Timer expirations**: Timer threads post `ElectionTimeoutEvent` or `HeartbeatTimeoutEvent` to the event queue.
3. **Client commands**: Any thread can call `submit_command()`, which posts work to the event queue.
4. **Event processing**: The single-threaded event loop dequeues events and dispatches them to the appropriate handler. All consensus logic runs here without fine-grained locking.
5. **Outgoing RPCs**: Handlers serialize messages via the Serializer and submit them to the Transport for asynchronous sending.
6. **Persistence**: Before replying to an AppendEntries, the LogStore and PersistentState are fsync'd. Before replying to a RequestVote with a grant, votedFor is durably saved.

## Logging and Metrics

### Logging

The logging subsystem is built on an abstract `Logger` interface backed by spdlog. Each node creates a logger prefixed with its node ID, producing output like:

```
[2025-01-15 10:23:45.123] [raft-node-1] [info] Starting election for term 5
[2025-01-15 10:23:45.130] [raft-node-1] [debug] Sending RequestVote to node 2
```

Log levels: `Trace`, `Debug`, `Info`, `Warn`, `Error`, `Fatal`. Set via `config.log_level`.

### Metrics

The `Metrics` interface provides counters and gauges. The default in-memory implementation stores values in mutex-protected maps. Key metrics:

**Counters:**
- `elections_started` -- number of elections initiated
- `elections_won` -- number of elections won
- `vote_requests_received` -- total RequestVote RPCs received
- `append_entries_received` -- total AppendEntries RPCs received
- `snapshots_taken` -- snapshots created
- `snapshots_installed` -- InstallSnapshot RPCs applied

**Gauges:**
- `current_term` -- this node's current term
- `commit_index` -- highest committed log index
- `last_applied` -- last index applied to the state machine
- `log_size` -- number of entries in the log
- `role` -- 0=Follower, 1=Candidate, 2=Leader
- `peers_count` -- current cluster size

For production, replace the default with a Prometheus or StatsD exporter by implementing the `Metrics` interface.

## Testing Strategy

The test suite is organized into two tiers:

### Unit Tests (`tests/unit/`)

Per-module tests that exercise individual components in isolation using mock implementations of injected interfaces:

- **RaftLog tests** -- append, truncate, snapshot compaction, term lookups
- **ClusterMembership tests** -- quorum checks, joint consensus transitions
- **Serializer tests** -- round-trip serialization of every message type
- **Timer tests** -- deterministic timer behavior with test doubles
- **PersistentState tests** -- atomic save/recovery, crash simulation
- **LogStore tests** -- file I/O, index rebuild, fsync correctness
- **SnapshotStore tests** -- chunked write/read, atomic finalization

### Integration Tests (`tests/integration/`)

End-to-end tests that spin up multiple RaftNode instances using in-process transports and validate cluster behavior:

| Test File                       | What It Tests                                        |
|---------------------------------|------------------------------------------------------|
| `test_leader_election.cpp`      | Single-round election, quorum, candidate timeout     |
| `test_log_replication.cpp`      | Entries replicated to majority, commit advancement   |
| `test_follower_catchup.cpp`     | Slow follower catches up after reconnection          |
| `test_leader_failover.cpp`      | Leader dies, new election, client redirected         |
| `test_stale_term.cpp`           | Stale leader stepped down by higher term             |
| `test_snapshot_install.cpp`     | Follower too far behind, receives snapshot           |
| `test_network_partition.cpp`    | Minority partition cannot elect leader               |
| `test_restart_recovery.cpp`     | Node restarts, recovers state from disk              |
| `test_membership_change.cpp`    | Joint consensus add/remove node                      |
| `test_client_interaction.cpp`   | Client submit, leader redirect, exactly-once         |

Run all tests:

```bash
cd build
ctest --output-on-failure
```

## Code Coverage

Coverage instrumentation uses gcov/lcov.

### Quick Start

```bash
chmod +x scripts/run_coverage.sh
./scripts/run_coverage.sh
```

### Manual Steps

```bash
# 1. Build with coverage
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DRAFT_ENABLE_COVERAGE=ON
cmake --build . -j$(nproc)

# 2. Run tests
ctest --output-on-failure

# 3. Capture coverage
lcov --capture --directory . --output-file coverage.info

# 4. Filter out third-party and test code
lcov --remove coverage.info \
    '/usr/*' \
    '*/third_party/*' \
    '*/tests/*' \
    --output-file filtered.info

# 5. Generate HTML report
genhtml filtered.info --output-directory coverage_html

# 6. Open in browser
xdg-open coverage_html/index.html  # Linux
# open coverage_html/index.html    # macOS
```

## Snapshot and Persistence

### Persistent State

Two pieces of state must be durable before any RPC reply is sent:

1. **currentTerm** -- The latest term the server has seen.
2. **votedFor** -- The candidate this node voted for in the current term.

These are saved atomically via write-to-temp, fsync, rename (`file_persistent_state.cpp`). A crash between fsync and rename is handled by checking for the temp file at startup.

### Log Persistence

Log entries are stored in an append-only file with a companion index file for O(1) random access. Every `append()` call is followed by an fsync before the method returns, guaranteeing durability.

### Snapshots

Snapshots capture the full state machine state at a point in time. They enable:
- **Log compaction** -- entries covered by the snapshot can be discarded.
- **Follower catch-up** -- via InstallSnapshot RPC when log entries have been compacted away.

Snapshot files are managed atomically: new snapshots are written to temp files and renamed into place. Incomplete snapshots from a crash are cleaned up at startup.

Storage layout:
```
data_dir/
├── state/        # currentTerm + votedFor (state.json)
├── log/          # Log entries (data file + index file)
└── snapshots/    # current.meta + current.data
```

## Safety Notes and Protocol Assumptions

### Assumptions

- **Crash-recovery model**: Nodes may crash and restart. They do not exhibit Byzantine faults.
- **Network model**: Asynchronous network. Messages may be delayed, reordered, or dropped. They are not corrupted (TCP guarantees integrity).
- **Majority availability**: The cluster makes progress as long as a strict majority of nodes are operational and can communicate.
- **Stable storage**: fsync guarantees durability. The underlying filesystem and disk honor fsync semantics.
- **Unique node IDs**: Each node has a unique, stable ID that persists across restarts.

### Safety Invariants

These invariants are guaranteed by a correct implementation:

1. **Election Safety**: At most one leader can be elected in a given term.
2. **Leader Append-Only**: A leader never overwrites or deletes entries in its log; it only appends new entries.
3. **Log Matching**: If two logs contain an entry with the same index and term, then the logs are identical in all preceding entries.
4. **Leader Completeness**: If a log entry is committed in a given term, that entry will be present in the logs of the leaders for all higher-numbered terms.
5. **State Machine Safety**: If a server has applied a log entry at a given index to its state machine, no other server will ever apply a different log entry for that same index.

### Critical Implementation Notes

- **Persist before reply**: currentTerm, votedFor, and log entries MUST be durably written before sending any RPC response.
- **Commit rule (Section 5.4.2)**: A leader must NOT commit entries from previous terms by counting replicas. It can only directly commit entries from its own term. Previous-term entries are committed indirectly.
- **Noop on election**: A new leader appends a Noop entry to ensure it can advance the commit index.
- **Atomic term+vote**: currentTerm and votedFor must be saved atomically. Writing one without the other can violate Election Safety.

## Extension Points

The interface-based architecture makes the following extensions straightforward:

| Extension                  | What to Do                                                       |
|----------------------------|------------------------------------------------------------------|
| **gRPC transport**         | Implement `Transport` using gRPC bidirectional streaming.        |
| **RocksDB storage**        | Implement `LogStore` and `PersistentState` backed by RocksDB.   |
| **Protobuf serialization** | Implement `Serializer` with .proto definitions.                  |
| **Key-value state machine**| Implement `StateMachine` with a `std::map` or similar.           |
| **Prometheus metrics**     | Implement `Metrics` with a Prometheus client library.            |
| **Pre-vote extension**     | Add a PreVote RPC phase before RequestVote to reduce disruption. |
| **Learner nodes**          | Extend `ClusterMembership` to track non-voting learners.         |
| **Read-only queries**      | Add a ReadIndex or LeaseRead mechanism for linearizable reads.   |
| **Batching**               | Batch multiple client commands into a single log entry.          |
| **Pipelining**             | Send multiple AppendEntries RPCs in flight per peer.             |

## Running a Local Cluster

See [docs/local_runtime_guide.md](docs/local_runtime_guide.md) for a full step-by-step walkthrough.

### Quick Start

```bash
# Build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DRAFT_BUILD_EXAMPLES=ON
cmake --build . -j$(nproc)
cd ..

# Launch 3 nodes (requires the example binaries to be built)
chmod +x scripts/launch_cluster.sh scripts/launch_client.sh
./scripts/launch_cluster.sh

# In another terminal, launch the client
./scripts/launch_client.sh
```

The cluster script launches 3 nodes on `127.0.0.1` ports 9001-9003 with data directories under `/tmp/raft_node_{1,2,3}`. Press Ctrl+C to kill all nodes.

See [docs/local_runtime_guide.md](docs/local_runtime_guide.md) for details on fault injection, killing nodes, and observing failover.

## License

This project is provided as an educational and development scaffold. See the LICENSE file for details.
