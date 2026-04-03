/**
 * @file raft_node.h
 * @brief The central Raft consensus module.
 *
 * RaftNode implements the core Raft algorithm.  It manages role transitions
 * (Follower ↔ Candidate ↔ Leader), processes incoming RPCs, drives outgoing
 * RPCs, and coordinates log replication, commitment, and application.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Architecture overview
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * Externally injected components (constructor):
 *   - Configuration     — node identity, timing, peers, paths
 *   - StateMachine      — application-level state (opaque to Raft)
 *   - Transport         — network layer for RPC
 *   - PersistentState   — durable currentTerm + votedFor
 *   - LogStore          — durable log entries
 *   - SnapshotStore     — durable snapshots
 *   - TimerFactory      — creates election / heartbeat timers
 *   - Serializer        — message encoding / decoding
 *   - Logger            — structured logging
 *   - Metrics           — counters and gauges
 *
 * Internal components (created by RaftNode):
 *   - RaftLog           — in-memory log cache on top of LogStore
 *   - ClusterMembership — tracks current and transitional configurations
 *   - election_timer_   — fires when followers haven't heard from leader
 *   - heartbeat_timer_  — fires on leader to send heartbeats
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Thread model
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 * The RaftNode runs a single-threaded event loop.  Incoming RPC callbacks
 * from the Transport and timer callbacks are posted to an internal queue and
 * processed serially.  This eliminates fine-grained locking inside the
 * consensus logic.
 *
 * The only mutex-protected interaction is the event queue itself:
 *   - Transport threads push incoming messages.
 *   - Timer threads push timeout events.
 *   - The main loop pops and processes events.
 *
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 * Raft invariants maintained
 * ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
 *
 *   1. Election safety:  at most one leader per term.
 *   2. Leader append-only:  a leader never overwrites or deletes entries;
 *      it only appends new entries.
 *   3. Log matching:  if two logs contain an entry with the same index and
 *      term, then the logs are identical in all entries up through that index.
 *   4. Leader completeness:  if a log entry is committed in a given term,
 *      then that entry will be present in the logs of the leaders for all
 *      higher-numbered terms.
 *   5. State machine safety:  if a server has applied a log entry at a given
 *      index, no other server will ever apply a different entry for that index.
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "raft/core/cluster_membership.h"
#include "raft/core/configuration.h"
#include "raft/core/raft_log.h"
#include "raft/core/state_machine.h"
#include "raft/rpc/messages.h"
#include "raft/rpc/transport.h"
#include "raft/storage/persistent_state.h"
#include "raft/storage/snapshot_store.h"
#include "raft/util/logger.h"
#include "raft/util/metrics.h"
#include "raft/util/serializer.h"
#include "raft/util/timer.h"
#include "raft/util/types.h"

namespace raft {

// ─── Per-peer replication state (volatile, leader only) ─────────────────────

/**
 * @brief Volatile leader state for a single peer.
 *
 * Per the Raft paper Figure 2:
 *   - nextIndex  — index of the next log entry to send to that server
 *                  (initialised to leader's last log index + 1)
 *   - matchIndex — highest log entry known to be replicated on that server
 *                  (initialised to 0)
 *
 * Additionally, tracks in-flight snapshot transfer state for InstallSnapshot.
 */
struct PeerState {
    LogIndex next_index{LogIndex{1}};
    LogIndex match_index{LogIndex::none()};

    /// If a snapshot is being streamed to this peer, this tracks the byte offset.
    std::optional<uint64_t> snapshot_offset;
};

// ─── Event types for the internal event queue ───────────────────────────────

struct IncomingMessage {
    NodeId from;
    rpc::MessageType type;
    std::vector<uint8_t> payload;
};

struct ElectionTimeoutEvent {};
struct HeartbeatTimeoutEvent {};
struct ShutdownEvent {};

/**
 * @brief Client request submitted via submit_command(), waiting for commit.
 */
struct PendingClientRequest {
    rpc::ClientRequest request;
    LogIndex log_index;

    /// Called when the entry is committed (or the request fails).
    std::function<void(rpc::ClientResponse)> on_complete;
};

using Event =
    std::variant<IncomingMessage, ElectionTimeoutEvent, HeartbeatTimeoutEvent, ShutdownEvent>;

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
// RaftNode
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

class RaftNode {
   public:
    /**
     * @brief Construct a RaftNode with all required components.
     *
     * The node does NOT start operating until start() is called.
     * All injected components must outlive the RaftNode.
     */
    RaftNode(Configuration config, std::unique_ptr<StateMachine> state_machine,
             std::unique_ptr<Transport> transport,
             std::unique_ptr<PersistentState> persistent_state, std::unique_ptr<LogStore> log_store,
             std::unique_ptr<SnapshotStore> snapshot_store,
             std::unique_ptr<TimerFactory> timer_factory, std::unique_ptr<Serializer> serializer,
             std::shared_ptr<Logger> logger, std::unique_ptr<Metrics> metrics);

    ~RaftNode();

    // Non-copyable, non-movable.
    RaftNode(const RaftNode&) = delete;
    RaftNode& operator=(const RaftNode&) = delete;

    // ── Lifecycle ───────────────────────────────────────────────────────

    /**
     * @brief Start the Raft node.
     *
     * Steps:
     *   1. Recover persistent state (term, votedFor, log, snapshot).
     *   2. Register with the transport and start listening.
     *   3. Add known peers to the transport.
     *   4. Start the election timeout timer.
     *   5. Launch the main event loop thread.
     *
     * @return Ok on success, or an error if recovery or listening fails.
     */
    Result<void> start();

    /**
     * @brief Gracefully shut down the Raft node.
     *
     * Posts a ShutdownEvent to the event queue, waits for the main loop to
     * exit, then stops the transport and timers.
     */
    void stop();

    /** @brief Returns true after start() and before stop() completes. */
    bool is_running() const;

    // ── Client interface ────────────────────────────────────────────────

    /**
     * @brief Submit a client command to the Raft cluster.
     *
     * If this node is the leader:
     *   1. Append the command as a new log entry.
     *   2. Begin replicating to followers.
     *   3. When committed, apply to the state machine and invoke the callback
     *      with the result.
     *
     * If this node is NOT the leader:
     *   - Immediately invoke the callback with NotLeader and a leader hint.
     *
     * Thread safety: may be called from any thread (posts to the event queue).
     *
     * @param request   The client's command.
     * @param callback  Called when the command is committed, fails, or this
     *                  node discovers it is not the leader.
     */
    void submit_command(rpc::ClientRequest request,
                        std::function<void(rpc::ClientResponse)> callback);

    // ── Observability ───────────────────────────────────────────────────

    /** @brief Current role (Follower, Candidate, Leader). */
    Role role() const;

    /** @brief Current term. */
    Term current_term() const;

    /** @brief The believed leader's ID, or nullopt if unknown. */
    std::optional<NodeId> leader_id() const;

    /** @brief This node's ID (from configuration). */
    NodeId id() const;

    /** @brief Current commit index. */
    LogIndex commit_index() const;

    /** @brief Last index applied to the state machine. */
    LogIndex last_applied() const;

   private:
    // ── Main event loop ─────────────────────────────────────────────────

    /**
     * @brief The main event processing loop, running on its own thread.
     *
     * Pops events from the queue and dispatches to the appropriate handler.
     * Exits when a ShutdownEvent is received.
     */
    void run_event_loop();

    /** @brief Post an event to the thread-safe queue. */
    void enqueue(Event event);

    // ── RPC handlers ────────────────────────────────────────────────────

    /**
     * @brief Dispatch an incoming message to the correct handler.
     *
     * Deserializes the payload and calls the appropriate handle_* method.
     * Called from the event loop (single-threaded).
     */
    void dispatch_message(const IncomingMessage& msg);

    /**
     * @brief Handle an incoming RequestVote RPC.
     *
     * Per §5.2 and §5.4.1 of the Raft paper:
     *
     *   1. If the request's term < currentTerm, reply with vote_granted=false.
     *
     *   2. If this server has already voted for a different candidate in the
     *      current term, reply false.
     *
     *   3. Check the "at least as up-to-date" condition:
     *      - Compare (lastLogTerm, lastLogIndex) of the candidate vs. this
     *        server.  A log is more up-to-date if:
     *          a) its last entry has a higher term, OR
     *          b) same last term but a higher last index.
     *      - If the candidate's log is NOT at least as up-to-date, reply false.
     *
     *   4. Otherwise, grant the vote:
     *      - Set votedFor = candidateId (persist before replying!).
     *      - Reset the election timer (§5.2: reset timer on granting vote).
     *      - Reply with vote_granted=true.
     *
     * Edge cases:
     *   - If request.term > currentTerm, step down to Follower and update
     *     term BEFORE evaluating the vote (§5.1 Rules for Servers).
     *   - If already voted for this same candidate in this term, grant again
     *     (idempotent — handles retransmissions).
     *
     * Invariant preserved: at most one vote granted per term.
     */
    void handle_request_vote(NodeId from, const rpc::RequestVote& req);

    /**
     * @brief Handle a RequestVoteReply from a peer.
     *
     * Only meaningful when this node is a Candidate.
     *
     *   1. If reply.term > currentTerm, step down to Follower.
     *
     *   2. If reply.term < currentTerm (stale reply), ignore.
     *
     *   3. If vote_granted, record the vote.  If a quorum of votes has been
     *      collected, transition to Leader:
     *      - Initialise nextIndex[] and matchIndex[] for all peers.
     *      - Append a Noop entry to the log (§8, commitIndex advancement).
     *      - Start the heartbeat timer.
     *      - Immediately send AppendEntries (heartbeat) to all peers.
     *
     * Edge case: votes from nodes not in the current configuration should be
     * ignored during a membership change.
     */
    void handle_request_vote_reply(NodeId from, const rpc::RequestVoteReply& reply);

    /**
     * @brief Handle an incoming AppendEntries RPC (or heartbeat).
     *
     * Per §5.3 of the Raft paper (receiver implementation):
     *
     *   1. If request.term < currentTerm, reply success=false.
     *
     *   2. Recognise the sender as the current leader; reset election timer.
     *
     *   3. If the log does not contain an entry at prevLogIndex with term
     *      prevLogTerm, reply success=false.  Optionally include conflict_term
     *      and conflict_index for the fast backtracking optimization.
     *
     *   4. For each entry in the request:
     *      - If the log already has an entry at the same index with the same
     *        term, skip it (idempotent).
     *      - If the log has an entry at the same index with a DIFFERENT term,
     *        delete that entry and all that follow it, then append the new
     *        entries.
     *      - If the log does not have an entry at this index, append.
     *
     *   5. If leaderCommit > commitIndex, set commitIndex =
     *      min(leaderCommit, index of last new entry).
     *
     *   6. Reply success=true.
     *
     * Edge cases:
     *   - Empty entries (heartbeat): still validate prevLogIndex/prevLogTerm
     *     and update commitIndex.
     *   - request.term > currentTerm: step down to Follower first.
     *   - Duplicate RPCs: the algorithm is idempotent by design.
     */
    void handle_append_entries(NodeId from, const rpc::AppendEntries& req);

    /**
     * @brief Handle an AppendEntriesReply from a peer.
     *
     * Only meaningful when this node is the Leader.
     *
     *   1. If reply.term > currentTerm, step down to Follower.
     *
     *   2. If reply.success == true:
     *      - Update matchIndex[peer] and nextIndex[peer].
     *      - Check if commitIndex can be advanced: find the highest N such
     *        that a majority of matchIndex[i] ≥ N AND log[N].term ==
     *        currentTerm (§5.4.2 — only commit entries from the current term).
     *
     *   3. If reply.success == false:
     *      - Use the optimized backtracking (conflict_term/conflict_index) to
     *        skip back efficiently, or just decrement nextIndex[peer].
     *      - Retry AppendEntries with the new nextIndex.
     *      - If nextIndex drops below the snapshot's lastIncludedIndex,
     *        switch to sending InstallSnapshot instead.
     *
     * Invariant: commitIndex never decreases.
     */
    void handle_append_entries_reply(NodeId from, const rpc::AppendEntriesReply& reply);

    /**
     * @brief Handle an incoming InstallSnapshot RPC.
     *
     * Per §7 of the Raft paper:
     *
     *   1. If request.term < currentTerm, reply immediately.
     *
     *   2. Reset election timer (the leader is alive).
     *
     *   3. Write the snapshot chunk to the SnapshotStore at the given offset.
     *      If offset == 0, begin a new snapshot.
     *
     *   4. If done == false, reply and wait for more chunks.
     *
     *   5. If done == true:
     *      a) Finalize the snapshot in the SnapshotStore.
     *      b) If the snapshot covers a prefix of the existing log, discard
     *         covered entries.  If it extends beyond the log, discard the
     *         entire log.
     *      c) Load the snapshot into the state machine via restore_snapshot().
     *      d) Update lastApplied and commitIndex.
     *
     * Edge case: if the receiver already has a snapshot with a higher
     * last_included_index, the incoming snapshot is stale — ignore.
     */
    void handle_install_snapshot(NodeId from, const rpc::InstallSnapshot& req);

    /**
     * @brief Handle an InstallSnapshotReply from a peer.
     *
     * Only meaningful when this node is the Leader.
     *
     *   1. If reply.term > currentTerm, step down.
     *
     *   2. If the snapshot transfer is complete (we sent done=true last time),
     *      update matchIndex and nextIndex for this peer.
     *
     *   3. If more chunks remain, send the next chunk.
     */
    void handle_install_snapshot_reply(NodeId from, const rpc::InstallSnapshotReply& reply);

    // ── Role transitions ────────────────────────────────────────────────

    /**
     * @brief Transition to Follower.
     *
     * Called when:
     *   - A higher term is discovered (from any RPC or reply).
     *   - A candidate loses an election.
     *   - A leader discovers a higher term.
     *
     * Actions:
     *   - Set role to Follower.
     *   - Update currentTerm to @p new_term (persist!).
     *   - Clear votedFor (persist atomically with term).
     *   - Stop the heartbeat timer (if it was running).
     *   - Reset the election timer.
     *   - Update the leader_id if known.
     *
     * @param new_term  The new (higher or equal) term.
     * @param leader    The known leader, or nullopt.
     */
    void become_follower(Term new_term, std::optional<NodeId> leader = std::nullopt);

    /**
     * @brief Start an election: transition to Candidate.
     *
     * Per §5.2:
     *   1. Increment currentTerm.
     *   2. Vote for self (persist!).
     *   3. Reset the election timer (with a new random timeout).
     *   4. Send RequestVote RPCs to all other servers.
     *
     * Invoked when the election timer fires while in Follower or Candidate
     * state (handles repeated election timeouts / split votes).
     */
    void start_election();

    /**
     * @brief Transition to Leader.
     *
     * Called when a Candidate wins the election (majority of votes received).
     *
     * Per §5.2 / Figure 2:
     *   1. Set role to Leader.
     *   2. Initialise nextIndex[peer] = last_log_index + 1 for all peers.
     *   3. Initialise matchIndex[peer] = 0 for all peers.
     *   4. Append a Noop entry (current term) to the log.
     *      Rationale (§8): ensures the new leader can commit entries from
     *      previous terms by first committing something from its own term.
     *   5. Stop the election timer.
     *   6. Start the heartbeat timer.
     *   7. Immediately send AppendEntries to all peers.
     *
     * Safety: only called when this node has received a strict majority of
     * votes in the current term.
     */
    void become_leader();

    // ── Leader operations ───────────────────────────────────────────────

    /**
     * @brief Send AppendEntries RPCs to all peers.
     *
     * For each peer:
     *   - Determine the entries to send: entries from nextIndex[peer]
     *     through lastLogIndex.
     *   - If nextIndex is before the snapshot, send InstallSnapshot instead.
     *   - Build the AppendEntries message with the correct
     *     prevLogIndex/prevLogTerm.
     *   - Serialize and send via the transport.
     *
     * This is called:
     *   - On heartbeat timeout.
     *   - After appending new entries (client command or membership change).
     *   - After receiving a successful AppendEntriesReply (to pipeline).
     */
    void send_append_entries_to_all();

    /**
     * @brief Send AppendEntries to a single peer.
     *
     * @param peer_id  The target peer.
     */
    void send_append_entries(NodeId peer_id);

    /**
     * @brief Send the next InstallSnapshot chunk to a peer.
     *
     * Used when a follower is too far behind and the required log entries
     * have been compacted away.
     */
    void send_install_snapshot(NodeId peer_id);

    /**
     * @brief Attempt to advance the commit index.
     *
     * Per §5.3 / §5.4.2:
     *   - Find the largest N > commitIndex such that:
     *       a) A majority of matchIndex[i] ≥ N.
     *       b) log[N].term == currentTerm.
     *   - Set commitIndex = N.
     *
     * The requirement that log[N].term == currentTerm prevents a new leader
     * from committing entries from a previous term by counting replicas;
     * the leader must first commit something from its own term (the Noop).
     *
     * After advancing commitIndex, apply all entries in
     * [lastApplied + 1, commitIndex] to the state machine.
     */
    void advance_commit_index();

    /**
     * @brief Apply committed but unapplied entries to the state machine.
     *
     * For each entry from lastApplied + 1 to commitIndex:
     *   1. Call state_machine_->apply(entry).
     *   2. Increment lastApplied.
     *   3. If the entry has a pending client callback, invoke it with the
     *      state machine's response.
     *
     * Postcondition: lastApplied == commitIndex.
     */
    void apply_committed_entries();

    // ── Snapshotting ────────────────────────────────────────────────────

    /**
     * @brief Check if a snapshot should be triggered and take one if so.
     *
     * A snapshot is triggered when the number of log entries since the last
     * snapshot exceeds config_.snapshot_threshold.
     *
     * Steps:
     *   1. Capture the state machine state via take_snapshot().
     *   2. Persist the snapshot via the SnapshotStore.
     *   3. Compact the log (discard entries included in the snapshot).
     */
    void maybe_take_snapshot();

    // ── Term management ─────────────────────────────────────────────────

    /**
     * @brief Check if a received term is higher than ours and step down.
     *
     * Per §5.1 "Rules for Servers":
     *   "If RPC request or response contains term T > currentTerm:
     *    set currentTerm = T, convert to follower."
     *
     * @param received_term  The term from an incoming RPC.
     * @return true if we stepped down (caller should likely abort processing).
     */
    bool maybe_step_down(Term received_term);

    // ── Helpers ─────────────────────────────────────────────────────────

    /** @brief Generate a random election timeout duration. */
    Timer::Duration random_election_timeout() const;

    /** @brief Reset the election timer with a new random duration. */
    void reset_election_timer();

    /** @brief Start the heartbeat timer (leader only). */
    void start_heartbeat_timer();

    /** @brief Stop the heartbeat timer. */
    void stop_heartbeat_timer();

    // ═══════════════════════════════════════════════════════════════════
    // Member data
    // ═══════════════════════════════════════════════════════════════════

    // ── Configuration (immutable after construction) ────────────────────
    Configuration config_;

    // ── Injected components ─────────────────────────────────────────────
    std::unique_ptr<StateMachine> state_machine_;
    std::unique_ptr<Transport> transport_;
    std::unique_ptr<PersistentState> persistent_state_;
    std::unique_ptr<SnapshotStore> snapshot_store_;
    std::unique_ptr<Serializer> serializer_;
    std::shared_ptr<Logger> logger_;
    std::unique_ptr<Metrics> metrics_;

    // ── Timers ──────────────────────────────────────────────────────────
    std::unique_ptr<TimerFactory> timer_factory_;
    std::unique_ptr<Timer> election_timer_;
    std::unique_ptr<Timer> heartbeat_timer_;

    // ── Log ─────────────────────────────────────────────────────────────
    std::unique_ptr<RaftLog> log_;

    // ── Persistent state (cached in-memory, always in sync with disk) ──
    Term current_term_{};
    std::optional<NodeId> voted_for_;

    // ── Volatile state (all servers) ────────────────────────────────────
    Role role_{Role::Follower};
    LogIndex commit_index_{LogIndex::none()};
    LogIndex last_applied_{LogIndex::none()};
    std::optional<NodeId> leader_id_;

    // ── Volatile state (leader only, reinitialised on election) ─────────
    std::unordered_map<NodeId, PeerState> peer_states_;

    // ── Election bookkeeping (candidate only) ───────────────────────────
    std::unordered_set<NodeId> votes_received_;

    // ── Cluster membership ──────────────────────────────────────────────
    ClusterMembership membership_;

    // ── Pending client requests (leader only) ───────────────────────────
    std::unordered_map<LogIndex, PendingClientRequest> pending_requests_;

    // ── Client session table for exactly-once semantics (§8) ────────────
    struct ClientSession {
        uint64_t last_sequence{0};
        std::vector<uint8_t> last_response;
    };
    std::unordered_map<uint64_t, ClientSession> client_sessions_;

    // ── Event queue (thread-safe) ───────────────────────────────────────
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::queue<Event> event_queue_;

    // ── Main loop thread ────────────────────────────────────────────────
    std::jthread event_loop_thread_;
    std::atomic<bool> running_{false};
};

}  // namespace raft
