/**
 * @file raft_node.cpp
 * @brief Implementation stubs for the central Raft consensus module.
 *
 * This file contains the stub implementations for every method declared in
 * raft_node.h.  Each method has a detailed comment block explaining the
 * intended behaviour, invariants, and edge cases.  The actual consensus
 * logic is left as TODO for the implementer.
 *
 * Implementation order recommendation:
 *   1. Lifecycle (start, stop, run_event_loop)
 *   2. Term management (maybe_step_down, become_follower)
 *   3. Leader election (start_election, handle_request_vote, handle_request_vote_reply,
 * become_leader)
 *   4. Log replication (handle_append_entries, handle_append_entries_reply, send_append_entries)
 *   5. Commit & apply (advance_commit_index, apply_committed_entries)
 *   6. Client commands (submit_command)
 *   7. Snapshotting (maybe_take_snapshot, handle_install_snapshot, send_install_snapshot)
 */

#include "raft/core/raft_node.h"

#include <random>
#include <stdexcept>

namespace raft {

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Construction / Destruction
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

RaftNode::RaftNode(Configuration config, std::unique_ptr<StateMachine> state_machine,
                   std::unique_ptr<Transport> transport,
                   std::unique_ptr<PersistentState> persistent_state,
                   std::unique_ptr<LogStore> log_store,
                   std::unique_ptr<SnapshotStore> snapshot_store,
                   std::unique_ptr<TimerFactory> timer_factory,
                   std::unique_ptr<Serializer> serializer, std::shared_ptr<Logger> logger,
                   std::unique_ptr<Metrics> metrics)
    : config_{std::move(config)},
      state_machine_{std::move(state_machine)},
      transport_{std::move(transport)},
      persistent_state_{std::move(persistent_state)},
      snapshot_store_{std::move(snapshot_store)},
      serializer_{std::move(serializer)},
      logger_{std::move(logger)},
      metrics_{std::move(metrics)},
      timer_factory_{std::move(timer_factory)},
      log_{std::make_unique<RaftLog>(std::move(log_store))} {}

RaftNode::~RaftNode() {
    stop();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Lifecycle
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Result<void> RaftNode::start() {
    // TODO: Implement startup sequence.
    //
    // Steps:
    //   1. Recover persistent state:
    //      - Read currentTerm and votedFor from persistent_state_.
    //      - Cache them in current_term_ and voted_for_.
    //
    //   2. Recover log:
    //      - The RaftLog constructor already loaded persisted entries.
    //      - If a snapshot exists, restore it into the state machine.
    //      - Set last_applied_ to the snapshot's last_included_index.
    //
    //   3. Initialise cluster membership from the configuration:
    //      - Build a ClusterConfig from config_.initial_peers.
    //      - If the log contains a more recent Config entry, use that instead.
    //
    //   4. Start the transport:
    //      - Call transport_->start() with this node's address.
    //      - The message handler should enqueue IncomingMessage events.
    //      - Add all peers to the transport.
    //
    //   5. Create timers:
    //      - election_timer_ = timer_factory_->create()
    //      - heartbeat_timer_ = timer_factory_->create()
    //
    //   6. Reset the election timer (start as Follower).
    //
    //   7. Launch the event loop thread.
    //
    //   8. Set running_ = true.
    //
    // Error handling:
    //   - If transport_->start() fails, return the error without launching threads.
    //   - If persistent state is corrupt, log a warning and start fresh.

    throw std::logic_error("RaftNode::start() not implemented");
}

void RaftNode::stop() {
    // TODO: Implement graceful shutdown.
    //
    // Steps:
    //   1. If not running, return immediately (idempotent).
    //   2. Set running_ = false.
    //   3. Post a ShutdownEvent to the event queue.
    //   4. Join the event loop thread (wait for it to exit).
    //   5. Stop timers.
    //   6. Stop the transport (closes all connections).
    //   7. Fail any pending client requests with ShuttingDown error.
}

bool RaftNode::is_running() const {
    return running_.load(std::memory_order_relaxed);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Event loop
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void RaftNode::run_event_loop() {
    // TODO: Implement the main event loop.
    //
    // Pseudocode:
    //   while (running_) {
    //       Event event;
    //       {
    //           std::unique_lock lock(queue_mutex_);
    //           queue_cv_.wait(lock, [&] { return !event_queue_.empty(); });
    //           event = std::move(event_queue_.front());
    //           event_queue_.pop();
    //       }
    //
    //       std::visit(overloaded{
    //           [this](IncomingMessage& msg)    { dispatch_message(msg); },
    //           [this](ElectionTimeoutEvent&)   { start_election(); },
    //           [this](HeartbeatTimeoutEvent&)  { send_append_entries_to_all(); },
    //           [this](ShutdownEvent&)          { running_ = false; },
    //       }, event);
    //
    //       // After processing any event, apply committed entries.
    //       apply_committed_entries();
    //
    //       // Check if we should take a snapshot.
    //       maybe_take_snapshot();
    //   }
    //
    // Notes:
    //   - The event loop is single-threaded by design.  All consensus logic
    //     runs here without fine-grained locking.
    //   - The only mutex is queue_mutex_, which protects the event queue.
    //   - Timer and transport callbacks post events to the queue from other threads.
}

void RaftNode::enqueue(Event event) {
    // TODO: Thread-safe event posting.
    //
    // Steps:
    //   1. Lock queue_mutex_.
    //   2. Push event onto event_queue_.
    //   3. Notify queue_cv_.
    //
    // This is called from transport threads and timer threads.
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Message dispatch
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void RaftNode::dispatch_message(const IncomingMessage& msg) {
    // TODO: Deserialize the payload and dispatch to the correct handler.
    //
    // Steps:
    //   1. Use serializer_->detect_type() — but we already have msg.type.
    //   2. Switch on msg.type and call the appropriate deserialization +
    //      handler method.
    //
    // Example:
    //   switch (msg.type) {
    //       case rpc::MessageType::RequestVote:
    //           handle_request_vote(msg.from,
    //               serializer_->deserialize_request_vote(msg.payload));
    //           break;
    //       case rpc::MessageType::RequestVoteReply:
    //           handle_request_vote_reply(msg.from,
    //               serializer_->deserialize_request_vote_reply(msg.payload));
    //           break;
    //       ... (all message types)
    //   }
    //
    // Error handling: if deserialization fails, log a warning and ignore
    // the message (do NOT crash).  Network corruption or protocol bugs
    // should be tolerable.
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  RequestVote RPC
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void RaftNode::handle_request_vote(NodeId from, const rpc::RequestVote& req) {
    // TODO: Implement RequestVote handler.
    //
    // Raft paper §5.2, §5.4.1:
    //
    //   1. Call maybe_step_down(req.term).  If we stepped down, our term
    //      is now req.term and votedFor is nullopt.
    //
    //   2. If req.term < current_term_, reply { current_term_, false }.
    //
    //   3. If we have already voted in this term for a different candidate,
    //      reply false.  (votedFor != nullopt && votedFor != req.candidate_id)
    //
    //   4. Check the "at least as up-to-date" condition:
    //      let our_last_term  = log_->last_term();
    //      let our_last_index = log_->last_index();
    //
    //      candidate_is_up_to_date =
    //          (req.last_log_term > our_last_term) ||
    //          (req.last_log_term == our_last_term && req.last_log_index >= our_last_index);
    //
    //      If NOT up-to-date, reply false.
    //
    //   5. Grant the vote:
    //      - voted_for_ = req.candidate_id
    //      - persistent_state_->save(current_term_, voted_for_)   ← MUST be durable before reply!
    //      - Reset election timer (granting a vote counts as hearing from a valid
    //      leader/candidate).
    //      - Reply { current_term_, true }.
    //
    //   6. Send the reply via transport_->send().
    //
    //   7. Update metrics: increment vote_requests_received.
    //
    // Edge case: if from == config_.node_id (voting for self), handle gracefully.
    // Edge case: if req.candidate_id is not in the current configuration during
    //            a membership change, consider ignoring (§4.2.3 disruption).
}

void RaftNode::handle_request_vote_reply(NodeId from, const rpc::RequestVoteReply& reply) {
    // TODO: Implement RequestVoteReply handler.
    //
    // Only process if role_ == Candidate.  Ignore stale replies.
    //
    //   1. Call maybe_step_down(reply.term).  If stepped down, return.
    //
    //   2. If reply.term != current_term_, ignore (stale).
    //
    //   3. If role_ != Candidate, ignore (already won or lost).
    //
    //   4. If reply.vote_granted:
    //      - Add `from` to votes_received_.
    //      - If membership_.has_quorum(votes_received_):
    //          → Call become_leader().
    //
    //   5. Update metrics.
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  AppendEntries RPC
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void RaftNode::handle_append_entries(NodeId from, const rpc::AppendEntries& req) {
    // TODO: Implement AppendEntries handler (Raft paper §5.3).
    //
    // This is the most complex RPC handler.  Follow these steps exactly:
    //
    //   1. Call maybe_step_down(req.term).
    //
    //   2. If req.term < current_term_, reply { current_term_, false }.
    //
    //   3. Recognise `from` as the leader:
    //      - leader_id_ = req.leader_id
    //      - If role_ == Candidate, become follower (we lost the election).
    //      - Reset election timer.
    //
    //   4. Log consistency check:
    //      - If req.prev_log_index > LogIndex::none():
    //          a) let existing_term = log_->term_at(req.prev_log_index);
    //          b) If existing_term is nullopt (we don't have this entry):
    //               → reply false with conflict_index = last_log_index + 1,
    //                 conflict_term = nullopt.
    //          c) If existing_term != req.prev_log_term:
    //               → Find the first index for that conflicting term.
    //               → reply false with conflict_index and conflict_term
    //                 set for the fast backtracking optimization.
    //
    //   5. Process entries:
    //      - For each entry in req.entries at index i:
    //          let entry_index = req.prev_log_index + 1 + i;
    //          a) If the log has an entry at entry_index with the same term,
    //             skip (already have it — idempotent).
    //          b) If the log has an entry at entry_index with a DIFFERENT term,
    //             truncate from entry_index onward, then append remaining.
    //          c) If the log has no entry at entry_index, append.
    //
    //   6. Update commit index:
    //      - If req.leader_commit > commit_index_:
    //          let new_commit = min(req.leader_commit, last new entry index);
    //          commit_index_ = new_commit;
    //
    //   7. Reply { current_term_, true }.
    //
    //   8. Check if any new entries are Config entries and update membership.
    //
    //   9. Update metrics: increment append_entries_received.
    //
    // Edge case: empty entries (heartbeat) — steps 5 is a no-op, but steps
    //            3, 4, 6 MUST still execute.
    // Edge case: the leader may send overlapping entries (some already in
    //            our log).  The algorithm handles this correctly by checking
    //            each entry individually.
}

void RaftNode::handle_append_entries_reply(NodeId from, const rpc::AppendEntriesReply& reply) {
    // TODO: Implement AppendEntriesReply handler (leader only).
    //
    //   1. If role_ != Leader, ignore.
    //
    //   2. Call maybe_step_down(reply.term).  If stepped down, return.
    //
    //   3. If reply.term != current_term_, ignore (stale).
    //
    //   4. Look up peer_states_[from] (the PeerState for this follower).
    //
    //   5. If reply.success:
    //      - Update matchIndex: the follower matched through the entries
    //        we sent.  matchIndex[from] = prev_log_index + number_of_entries_sent.
    //      - Update nextIndex:  nextIndex[from] = matchIndex[from] + 1.
    //      - Call advance_commit_index().
    //      - If more entries are available, pipeline another AppendEntries.
    //
    //   6. If !reply.success:
    //      - Fast backtracking (optimized):
    //          If reply.conflict_term has a value:
    //              Search our log for the last entry with that term.
    //              If found, set nextIndex = that index + 1.
    //              If not found, set nextIndex = reply.conflict_index.
    //          Else:
    //              nextIndex[from] = reply.conflict_index (or nextIndex - 1).
    //      - Clamp: nextIndex must be >= 1.
    //      - If nextIndex has dropped below the snapshot's last_included_index,
    //        switch to sending InstallSnapshot.
    //      - Otherwise, retry send_append_entries(from).
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  InstallSnapshot RPC
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void RaftNode::handle_install_snapshot(NodeId from, const rpc::InstallSnapshot& req) {
    // TODO: Implement InstallSnapshot handler (Raft paper §7).
    //
    //   1. Call maybe_step_down(req.term).
    //
    //   2. If req.term < current_term_, reply { current_term_ }.
    //
    //   3. Reset election timer (leader is alive).
    //
    //   4. If we already have a snapshot with last_included_index >=
    //      req.last_included_index, the incoming snapshot is stale.  Reply OK.
    //
    //   5. If req.offset == 0:
    //      - Begin a new snapshot via snapshot_store_->begin_snapshot().
    //
    //   6. Write the chunk:
    //      - snapshot_store_->write_chunk(req.offset, req.data).
    //
    //   7. If req.done == false:
    //      - Reply { current_term_ } and wait for more chunks.
    //
    //   8. If req.done == true:
    //      a) Finalize: snapshot_store_->finalize_snapshot().
    //      b) Restore state machine: state_machine_->restore_snapshot(full_data).
    //      c) Update log:
    //           - If the log contains an entry at req.last_included_index
    //             with term == req.last_included_term, keep entries after it.
    //           - Otherwise, discard the entire log.
    //           - log_->apply_snapshot(req.last_included_index, req.last_included_term).
    //      d) Update volatile state:
    //           - last_applied_ = req.last_included_index
    //           - commit_index_ = max(commit_index_, req.last_included_index)
    //      e) Update persistent state if the snapshot's term > current_term_.
    //
    //   9. Reply { current_term_ }.
}

void RaftNode::handle_install_snapshot_reply(NodeId from, const rpc::InstallSnapshotReply& reply) {
    // TODO: Implement InstallSnapshotReply handler (leader only).
    //
    //   1. If role_ != Leader, ignore.
    //   2. Call maybe_step_down(reply.term).  If stepped down, return.
    //   3. Look up peer_states_[from].
    //   4. If we just sent the final chunk (done=true):
    //      - Update nextIndex[from] = snapshot.last_included_index + 1.
    //      - Update matchIndex[from] = snapshot.last_included_index.
    //      - Clear the snapshot_offset tracker.
    //      - Continue with AppendEntries for remaining entries.
    //   5. If more chunks remain, send the next chunk.
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Role transitions
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void RaftNode::become_follower(Term new_term, std::optional<NodeId> leader) {
    // TODO: Implement Follower transition.
    //
    //   1. Log the transition: "Becoming follower in term {new_term}".
    //
    //   2. role_ = Role::Follower.
    //
    //   3. If new_term > current_term_:
    //      - current_term_ = new_term
    //      - voted_for_ = std::nullopt
    //      - persistent_state_->save(current_term_, voted_for_)  ← durable!
    //
    //   4. leader_id_ = leader.
    //
    //   5. Stop heartbeat timer (only leaders send heartbeats).
    //
    //   6. Reset election timer.
    //
    //   7. Clear votes_received_ (no longer a candidate).
    //
    //   8. Clear peer_states_ (no longer a leader).
    //
    //   9. Fail any pending client requests with NotLeader.
    //
    //  10. Update metrics: set role gauge to 0 (follower).
}

void RaftNode::start_election() {
    // TODO: Implement election startup (Raft paper §5.2).
    //
    //   1. Increment current_term_.
    //
    //   2. Transition to Candidate:
    //      - role_ = Role::Candidate.
    //      - voted_for_ = config_.node_id  (vote for self).
    //      - persistent_state_->save(current_term_, voted_for_)  ← durable!
    //
    //   3. Reset votes:
    //      - votes_received_.clear()
    //      - votes_received_.insert(config_.node_id)  (own vote).
    //
    //   4. Check if we already have a quorum (single-node cluster):
    //      - If membership_.has_quorum(votes_received_), become_leader().
    //      - return.
    //
    //   5. Reset election timer (new random timeout for split-vote retry).
    //
    //   6. Build a RequestVote message:
    //      - term = current_term_
    //      - candidate_id = config_.node_id
    //      - last_log_index = log_->last_index()
    //      - last_log_term  = log_->last_term()
    //
    //   7. Serialize and send to all peers in membership_.all_voting_members()
    //      (except self).
    //
    //   8. Log: "Starting election for term {current_term_}".
    //
    //   9. Update metrics: increment elections_started.
}

void RaftNode::become_leader() {
    // TODO: Implement Leader transition (Raft paper §5.2, Figure 2).
    //
    //   1. Assert: role_ == Candidate (safety check).
    //
    //   2. Log: "Elected leader for term {current_term_}".
    //
    //   3. role_ = Role::Leader.
    //   4. leader_id_ = config_.node_id.
    //
    //   5. Initialise peer_states_ for all peers in the cluster:
    //      For each peer != self:
    //          peer_states_[peer_id] = {
    //              .next_index  = log_->last_index() + LogIndex{1},
    //              .match_index = LogIndex::none(),
    //          };
    //
    //   6. Append a Noop entry:
    //      - LogEntry{ .term = current_term_, .type = EntryType::Noop }
    //      - This is critical: a new leader cannot commit entries from
    //        previous terms directly.  The Noop entry, once committed,
    //        indirectly commits all prior entries (§5.4.2, §8).
    //
    //   7. Stop election timer.
    //
    //   8. Start heartbeat timer.
    //
    //   9. Immediately send AppendEntries to all peers.
    //
    //  10. Update metrics: increment elections_won, set role gauge to 2 (leader).
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Leader operations
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void RaftNode::send_append_entries_to_all() {
    // TODO: Iterate over all peers and call send_append_entries(peer_id).
    //
    // Only call this when role_ == Leader.
    //
    // For each peer in membership_.all_voting_members() (except self):
    //   - If the peer needs a snapshot (nextIndex < snapshot_last_included_index),
    //     call send_install_snapshot(peer_id) instead.
    //   - Otherwise, call send_append_entries(peer_id).
    //
    // After sending, reset the heartbeat timer so the next heartbeat fires
    // after the configured interval.
}

void RaftNode::send_append_entries(NodeId peer_id) {
    // TODO: Build and send an AppendEntries RPC to a single peer.
    //
    //   1. Look up peer_states_[peer_id].
    //
    //   2. Determine prevLogIndex = nextIndex - 1.
    //      Determine prevLogTerm  = log_->term_at(prevLogIndex).value_or(Term{0}).
    //
    //   3. Collect entries from nextIndex through last_log_index.
    //      Limit the batch size to avoid huge RPCs (e.g. 100 entries or 1 MiB).
    //
    //   4. Build AppendEntries message:
    //      {
    //          .term           = current_term_,
    //          .leader_id      = config_.node_id,
    //          .prev_log_index = prevLogIndex,
    //          .prev_log_term  = prevLogTerm,
    //          .entries        = collected_entries,
    //          .leader_commit  = commit_index_,
    //      }
    //
    //   5. Serialize and send via transport_->send(peer_id, ...).
}

void RaftNode::send_install_snapshot(NodeId peer_id) {
    // TODO: Send the next InstallSnapshot chunk to a lagging peer.
    //
    //   1. Look up peer_states_[peer_id].snapshot_offset (default 0).
    //
    //   2. Read a chunk from snapshot_store_->read_chunk(offset, chunk_size).
    //
    //   3. Determine if this is the last chunk:
    //      done = (offset + chunk.size() >= snapshot_store_->snapshot_size()).
    //
    //   4. Build InstallSnapshot message with the snapshot metadata.
    //
    //   5. Serialize and send.
    //
    //   6. Update peer_states_[peer_id].snapshot_offset for the next call.
}

void RaftNode::advance_commit_index() {
    // TODO: Attempt to advance commitIndex (Raft paper §5.3, §5.4.2).
    //
    // This is called after matchIndex is updated for any peer.
    //
    //   1. For N from last_log_index down to commit_index_ + 1:
    //      a) Count how many peers have matchIndex >= N (including self).
    //      b) If membership_.has_quorum(that set):
    //         c) Check: log_->term_at(N) == current_term_.
    //            ← CRITICAL: a leader must NOT commit entries from previous
    //              terms by counting replicas.  It can only commit entries
    //              from its own term.  Previous-term entries are committed
    //              indirectly when a current-term entry is committed.
    //         d) If yes: commit_index_ = N.  Break.
    //         e) If no:  continue searching (there might be a current-term
    //            entry at a lower index).
    //
    //   2. Update metrics: set commit_index gauge.
    //
    // Optimisation: instead of scanning from the end, collect all matchIndexes
    // into a sorted array and find the quorum median.
}

void RaftNode::apply_committed_entries() {
    // TODO: Apply entries in (last_applied_, commit_index_] to the state machine.
    //
    //   while (last_applied_ < commit_index_) {
    //       last_applied_++;
    //       auto entry = log_->entry_at(last_applied_);
    //       // entry should always exist (committed entries are never compacted
    //       // before being applied).
    //
    //       // Apply to state machine:
    //       auto result = state_machine_->apply(
    //           entry->index, entry->term, entry->type,
    //           std::span<const uint8_t>(entry->data));
    //
    //       // If this entry has a pending client callback (leader only):
    //       if (auto it = pending_requests_.find(last_applied_);
    //           it != pending_requests_.end()) {
    //           // Check client session for exactly-once semantics:
    //           //   - If sequence_number <= last_sequence for this client,
    //           //     return the cached response (duplicate).
    //           //   - Otherwise, update the session and return the new response.
    //           auto& pending = it->second;
    //           rpc::ClientResponse resp;
    //           resp.success = true;
    //           resp.response = result;
    //           pending.on_complete(std::move(resp));
    //           pending_requests_.erase(it);
    //       }
    //
    //       // Handle Config entries:
    //       if (entry->type == EntryType::Config) {
    //           // If this was a C_old,new entry being committed, append
    //           // a C_new entry to complete the transition.
    //           // If this was a C_new entry being committed, finalize
    //           // the membership change.
    //       }
    //   }
    //
    //   // Update metrics: set last_applied gauge.
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Client interface
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void RaftNode::submit_command(rpc::ClientRequest request,
                              std::function<void(rpc::ClientResponse)> callback) {
    // TODO: Accept a client command.
    //
    // Thread safety: this method may be called from any thread.
    // It must post the work to the event loop.
    //
    //   1. If role_ != Leader:
    //      - Invoke callback immediately with:
    //        { .success = false, .error = NotLeader, .leader_hint = leader_id_ }
    //      - Return.
    //
    //   2. Check client session for exactly-once semantics:
    //      - If this client_id + sequence_number has already been applied,
    //        return the cached result.
    //
    //   3. Append the command to the log:
    //      LogEntry{ .term = current_term_, .type = EntryType::Command, .data = request.command }
    //
    //   4. Store the pending request so we can invoke the callback when committed.
    //
    //   5. Send AppendEntries to all peers to begin replication.
    //
    // Note: the callback is invoked later from the event loop thread when
    // the entry is committed.  If leadership is lost before then, fail the
    // request with NotLeader.
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Snapshotting
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

void RaftNode::maybe_take_snapshot() {
    // TODO: Check if a snapshot should be triggered and take one.
    //
    //   1. If config_.snapshot_threshold == 0, automatic snapshotting is disabled.
    //
    //   2. Compute entries_since_snapshot:
    //      If a snapshot exists: last_applied_ - snapshot.last_included_index.
    //      Else: last_applied_.value.
    //
    //   3. If entries_since_snapshot < snapshot_threshold, return.
    //
    //   4. Take snapshot:
    //      a) auto data = state_machine_->take_snapshot();
    //      b) Build SnapshotMetadata:
    //           - last_included_index = last_applied_
    //           - last_included_term  = log_->term_at(last_applied_)
    //           - configuration       = membership_.serialize()
    //      c) snapshot_store_->begin_snapshot(meta);
    //         snapshot_store_->write_chunk(0, data);
    //         snapshot_store_->finalize_snapshot();
    //      d) log_->compact(last_applied_, term);
    //
    //   5. Log: "Snapshot taken at index {last_applied_}".
    //   6. Update metrics: increment snapshots_taken.
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Term management
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

bool RaftNode::maybe_step_down(Term received_term) {
    // TODO: Implement the universal term-check rule.
    //
    // Per §5.1 "Rules for Servers":
    //   "If RPC request or response contains term T > currentTerm:
    //    set currentTerm = T, convert to follower."
    //
    //   if (received_term > current_term_) {
    //       become_follower(received_term);
    //       return true;
    //   }
    //   return false;
    return false;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Helpers
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Timer::Duration RaftNode::random_election_timeout() const {
    // TODO: Generate a random duration in [election_timeout_min, election_timeout_max].
    //
    // Use a thread-local random engine for thread safety.
    // std::uniform_int_distribution is appropriate.
    return config_.timers.election_timeout_min;
}

void RaftNode::reset_election_timer() {
    // TODO: Reset the election timer with a new random duration.
    //
    //   election_timer_->reset(random_election_timeout(), [this] {
    //       enqueue(ElectionTimeoutEvent{});
    //   });
}

void RaftNode::start_heartbeat_timer() {
    // TODO: Start the heartbeat timer (leader only).
    //
    //   heartbeat_timer_->reset(config_.timers.heartbeat_interval, [this] {
    //       enqueue(HeartbeatTimeoutEvent{});
    //   });
}

void RaftNode::stop_heartbeat_timer() {
    // TODO: heartbeat_timer_->stop();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  Observability
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Role RaftNode::role() const {
    return role_;
}
Term RaftNode::current_term() const {
    return current_term_;
}
std::optional<NodeId> RaftNode::leader_id() const {
    return leader_id_;
}
NodeId RaftNode::id() const {
    return config_.node_id;
}
LogIndex RaftNode::commit_index() const {
    return commit_index_;
}
LogIndex RaftNode::last_applied() const {
    return last_applied_;
}

}  // namespace raft
