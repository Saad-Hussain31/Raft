# Local Runtime Guide

Step-by-step instructions for building the project, running a 3-node Raft cluster on a single machine, interacting with it via a client, and observing fault tolerance in action.

## Prerequisites

- C++20 compiler (GCC 12+ or Clang 14+)
- CMake 3.22+
- Git (for submodules)
- 4 terminal windows (3 nodes + 1 client)

## Step 1: Clone and Initialize

```bash
git clone <repository-url> Raft
cd Raft
git submodule update --init --recursive
```

## Step 2: Build the Project

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DRAFT_BUILD_TESTS=ON \
         -DRAFT_BUILD_EXAMPLES=ON
cmake --build . -j$(nproc)
```

Verify the build produced the example binaries:

```bash
ls -la examples/raft_node examples/raft_client
```

## Step 3: Prepare Data Directories

Each node needs its own data directory for persistent state, logs, and snapshots.

```bash
mkdir -p /tmp/raft_node_1
mkdir -p /tmp/raft_node_2
mkdir -p /tmp/raft_node_3
```

## Step 4: Launch the Cluster

Open three terminal windows. The cluster consists of nodes 1-3 on ports 9001-9003.

### Terminal 1 -- Node 1

```bash
cd /path/to/Raft/build
./examples/raft_node \
    --id 1 \
    --listen 127.0.0.1:9001 \
    --peers 1=127.0.0.1:9001,2=127.0.0.1:9002,3=127.0.0.1:9003 \
    --data-dir /tmp/raft_node_1 \
    --log-level debug
```

### Terminal 2 -- Node 2

```bash
cd /path/to/Raft/build
./examples/raft_node \
    --id 2 \
    --listen 127.0.0.1:9002 \
    --peers 1=127.0.0.1:9001,2=127.0.0.1:9002,3=127.0.0.1:9003 \
    --data-dir /tmp/raft_node_2 \
    --log-level debug
```

### Terminal 3 -- Node 3

```bash
cd /path/to/Raft/build
./examples/raft_node \
    --id 3 \
    --listen 127.0.0.1:9003 \
    --peers 1=127.0.0.1:9001,2=127.0.0.1:9002,3=127.0.0.1:9003 \
    --data-dir /tmp/raft_node_3 \
    --log-level debug
```

### Using the Launch Script

Alternatively, use the provided script to launch all three nodes in background processes:

```bash
chmod +x scripts/launch_cluster.sh
./scripts/launch_cluster.sh
```

The script creates data directories, launches all nodes, and traps Ctrl+C to kill them all cleanly.

## Step 5: Observe Leader Election

Within a few hundred milliseconds of all three nodes starting, you should see log output indicating:

1. One node starts an election (its election timer fired first).
2. The other two nodes receive RequestVote RPCs and grant their votes.
3. The winning candidate transitions to Leader and begins sending heartbeats.

Example log output on the leader:

```
[2025-01-15 10:23:45.150] [raft-node-2] [info] Starting election for term 1
[2025-01-15 10:23:45.155] [raft-node-2] [info] Received vote from node 1 (2/3 votes)
[2025-01-15 10:23:45.157] [raft-node-2] [info] Received vote from node 3 (3/3 votes)
[2025-01-15 10:23:45.157] [raft-node-2] [info] Elected leader for term 1
```

Example log output on a follower:

```
[2025-01-15 10:23:45.153] [raft-node-1] [info] Granting vote to node 2 for term 1
[2025-01-15 10:23:45.160] [raft-node-1] [info] Recognized node 2 as leader for term 1
```

## Step 6: Launch the Client

Open a fourth terminal window.

```bash
cd /path/to/Raft/build
./examples/raft_client --target 127.0.0.1:9001
```

Or use the script:

```bash
chmod +x scripts/launch_client.sh
./scripts/launch_client.sh
```

If the client connects to a follower, it will receive a `NotLeader` response with a `leader_hint` and should automatically redirect to the leader.

## Step 7: Send Commands

With the client running, submit commands (assuming a key-value state machine):

```
> SET mykey myvalue
OK

> GET mykey
myvalue

> SET counter 42
OK
```

Each command is:
1. Received by the leader.
2. Appended to the leader's log.
3. Replicated to followers via AppendEntries.
4. Committed once a majority (2 of 3) have acknowledged.
5. Applied to the state machine.
6. Response sent back to the client.

## Step 8: Kill a Node and Watch Failover

### Kill a Follower

Kill one of the follower nodes (e.g., Node 3) with Ctrl+C or `kill`. The cluster should continue operating normally:

- The leader detects the follower is unresponsive (AppendEntries RPCs timeout).
- The remaining two nodes (majority: 2 of 3) can still commit entries.
- Client operations continue uninterrupted.

```bash
# Find and kill node 3
kill $(pgrep -f "raft_node.*--id 3")
```

Verify the cluster is still functional:

```
> SET after_kill value1
OK
```

### Kill the Leader

Kill the current leader node with Ctrl+C or `kill`:

```bash
# If node 2 was the leader:
kill $(pgrep -f "raft_node.*--id 2")
```

What happens next:

1. The remaining followers stop receiving heartbeats.
2. After their election timeout expires, one of them starts an election.
3. The surviving follower grants its vote.
4. A new leader is elected (in term 2).
5. The client detects the leader is gone (connection timeout or explicit rejection).
6. The client retries with the other known nodes.
7. The new leader begins serving requests.

Expected log output:

```
[2025-01-15 10:24:10.500] [raft-node-1] [warn] Election timeout - no heartbeat from leader
[2025-01-15 10:24:10.501] [raft-node-1] [info] Starting election for term 2
[2025-01-15 10:24:10.510] [raft-node-1] [info] Elected leader for term 2
```

**Note**: With only 2 of 3 nodes alive, the cluster still has a majority and can operate. If you killed 2 nodes, the remaining node cannot form a quorum and the cluster stalls until one is restarted.

## Step 9: Restart the Killed Node

Restart the killed node with the same arguments:

```bash
cd /path/to/Raft/build
./examples/raft_node \
    --id 3 \
    --listen 127.0.0.1:9003 \
    --peers 1=127.0.0.1:9001,2=127.0.0.1:9002,3=127.0.0.1:9003 \
    --data-dir /tmp/raft_node_3 \
    --log-level debug
```

What happens:

1. The node reads its persistent state (currentTerm, votedFor) from `/tmp/raft_node_3/state/`.
2. It loads its log from `/tmp/raft_node_3/log/`.
3. If a snapshot exists, it restores the state machine from `/tmp/raft_node_3/snapshots/`.
4. It starts as a Follower.
5. The current leader detects the reconnected node.
6. The leader sends AppendEntries with entries the node missed.
7. The node catches up and resumes normal operation.

If the node missed many entries and the leader has already taken a snapshot covering those entries, the leader sends an InstallSnapshot RPC instead.

## Step 10: Clean Up

Kill all nodes:

```bash
# If using the launch script, Ctrl+C in the script terminal.
# Or manually:
pkill -f "raft_node"
```

Clean up data directories:

```bash
rm -rf /tmp/raft_node_1 /tmp/raft_node_2 /tmp/raft_node_3
```

---

## Fault Injection Ideas

### Network Partition Simulation

Use `iptables` (Linux) to drop packets between specific nodes:

```bash
# Isolate node 3 from nodes 1 and 2 (drop all traffic on port 9003)
sudo iptables -A INPUT -p tcp --dport 9003 -j DROP
sudo iptables -A OUTPUT -p tcp --sport 9003 -j DROP

# Observe: node 3 starts elections but cannot win (no quorum).
# Nodes 1 and 2 continue operating with a majority.

# Remove the partition:
sudo iptables -D INPUT -p tcp --dport 9003 -j DROP
sudo iptables -D OUTPUT -p tcp --sport 9003 -j DROP
```

### Delayed Messages

Use `tc` (traffic control) to add latency:

```bash
# Add 200ms delay to loopback (affects all local traffic)
sudo tc qdisc add dev lo root netem delay 200ms

# Observe: elections take longer, but the protocol still works.
# The election timeout should be increased to accommodate the extra latency.

# Remove:
sudo tc qdisc del dev lo root
```

### Process Pause (Simulating GC Pause or Freeze)

```bash
# Pause node 2 (sends SIGSTOP)
kill -STOP $(pgrep -f "raft_node.*--id 2")

# Observe: the cluster treats node 2 as unresponsive.
# If node 2 was the leader, a new election occurs.

# Resume node 2 (sends SIGCONT)
kill -CONT $(pgrep -f "raft_node.*--id 2")

# Observe: node 2 discovers it is behind and catches up.
# If it was the old leader, it steps down (its term is stale).
```

### Disk Full Simulation

```bash
# Fill the data directory with a large file to exhaust disk
dd if=/dev/zero of=/tmp/raft_node_1/filler bs=1M count=1024

# Observe: the node fails to persist entries and should log errors.
# It should NOT crash -- it should return IoError from storage operations.

# Clean up:
rm /tmp/raft_node_1/filler
```

### Rapid Leader Kills

Kill the leader repeatedly to observe multiple re-elections:

```bash
for i in $(seq 1 5); do
    sleep 3
    LEADER_PID=$(pgrep -f "raft_node.*--id $(curl -s localhost:9001/leader)")
    kill $LEADER_PID
    echo "Killed leader (iteration $i)"
done
```

### Clock Skew

Run nodes with different system clocks (requires separate network namespaces or VMs). This tests whether the protocol correctly handles servers with mismatched wall-clock times. Raft is designed to be clock-independent (only uses relative timeouts), so it should tolerate arbitrary clock skew.

---

## Monitoring

### Log Output

All three nodes log to stdout with color-coded severity. Run each node in its own terminal to see the interleaved output.

For file-based logging, redirect output:

```bash
./examples/raft_node --id 1 ... 2>&1 | tee /tmp/raft_node_1.log
```

### Metrics

If an HTTP metrics endpoint is implemented, query it:

```bash
# Example: node 1's metrics
curl http://127.0.0.1:9101/metrics

# Expected output:
# elections_started 2
# elections_won 1
# current_term 3
# commit_index 42
# last_applied 42
# role 2
# log_size 37
```

---

## Troubleshooting

### No Leader Elected

- **Cause**: Nodes cannot reach each other on the configured ports.
- **Fix**: Verify all nodes are started and the peer addresses are correct. Check for firewall rules.

### Client Gets NotLeader Continuously

- **Cause**: The client is contacting the same follower repeatedly.
- **Fix**: Ensure the client follows `leader_hint` from the response to redirect to the actual leader.

### Node Fails to Start (Port Already in Use)

- **Cause**: A previous instance is still running or the port is held by TIME_WAIT.
- **Fix**: `kill` the old process or wait 60 seconds. The transport sets `SO_REUSEADDR` to mitigate this.

### Node Logs "Election timeout" Repeatedly

- **Cause**: The node is partitioned from the leader or the leader has died.
- **Expected**: If no quorum is available, the node will cycle through elections indefinitely. This is correct behavior -- the cluster is unavailable until a majority is restored.

### State Machine Divergence

- **Cause**: Non-deterministic state machine implementation.
- **Fix**: The `apply()` method MUST be deterministic. Given the same sequence of entries, every node must produce the same state. Avoid using random numbers, wall-clock time, or external I/O inside `apply()`.
