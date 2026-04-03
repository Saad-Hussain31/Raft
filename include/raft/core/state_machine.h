/**
 * @file state_machine.h
 * @brief Application state machine interface.
 *
 * The state machine is the application-level component that gives meaning to
 * replicated log entries.  The Raft consensus module replicates a totally-
 * ordered sequence of commands; the state machine applies them deterministically.
 *
 * Contract:
 *   - apply() is called exactly once per committed entry, in index order.
 *   - apply() must be deterministic: given the same sequence of commands,
 *     every node must arrive at the same state.
 *   - take_snapshot() captures the current state for log compaction.
 *   - restore_snapshot() replaces the current state from a snapshot.
 *
 * The Raft library does NOT know what the commands mean.  It treats command
 * data as opaque byte vectors.  The state machine interprets them.
 *
 * Example state machines:
 *   - A key-value store
 *   - A distributed lock manager
 *   - A replicated configuration store
 */

#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "raft/util/types.h"

namespace raft {

/**
 * @brief Abstract state machine interface.
 *
 * Thread safety: apply() is called from the main Raft thread, sequentially.
 * Snapshot operations may be called from a background thread if the node
 * supports concurrent snapshotting; document if your implementation requires
 * external synchronization.
 */
class StateMachine {
   public:
    virtual ~StateMachine() = default;

    /**
     * @brief Apply a committed command to the state machine.
     *
     * Called exactly once per committed entry, in strictly increasing index
     * order.  Noop entries (EntryType::Noop) should be applied as well —
     * the state machine can simply ignore them, but the index must still
     * advance.
     *
     * @param index   The log index of this entry.
     * @param term    The term in which this entry was created.
     * @param type    The entry type (Command, Noop, Config).
     * @param data    The opaque command bytes.  Empty for Noop.
     * @return The result of applying the command (returned to the client).
     */
    virtual std::vector<uint8_t> apply(LogIndex index, Term term, EntryType type,
                                       std::span<const uint8_t> data) = 0;

    /**
     * @brief Capture the current state machine state as a snapshot.
     *
     * The returned bytes must be sufficient to fully reconstruct the state
     * machine up through the last applied entry.  The Raft node will persist
     * these bytes via the SnapshotStore.
     *
     * This can be expensive.  Consider running it in the background if
     * your state machine supports copy-on-write.
     *
     * @return The serialized state machine state.
     */
    virtual std::vector<uint8_t> take_snapshot() const = 0;

    /**
     * @brief Replace the current state machine state with a snapshot.
     *
     * After this call, the state machine must behave exactly as if it had
     * applied all entries up through the snapshot's last_included_index.
     *
     * @param data  The snapshot bytes as produced by take_snapshot() (or
     *              received via InstallSnapshot).
     * @return Ok on success, IoError if the snapshot is malformed.
     */
    virtual Result<void> restore_snapshot(std::span<const uint8_t> data) = 0;

    /**
     * @brief Return the index of the last entry applied to this state machine.
     *
     * Used during recovery to avoid re-applying entries.
     */
    virtual LogIndex last_applied() const = 0;
};

}  // namespace raft
