/**
 * @file persistent_state.h
 * @brief Persistent state that must survive crashes (§5.2 Figure 2).
 *
 * The Raft paper requires three pieces of state to be durable:
 *   1. currentTerm  — the latest term the server has seen
 *   2. votedFor     — the candidateId that received this node's vote in the
 *                     current term, or nullopt if none
 *   3. log[]        — the replicated log (handled separately by LogStore)
 *
 * This interface handles (1) and (2).  They MUST be persisted atomically —
 * writing term without votedFor (or vice versa) can violate the voting
 * invariant and cause two leaders in the same term.
 *
 * Implementation strategy:
 *   - Write both fields to a single small file.
 *   - Use write-to-temp + fsync + rename for atomic updates.
 *   - On recovery, read the file and return the stored values.
 *   - If the file is missing or corrupt, treat as term=0, votedFor=nullopt
 *     (fresh node).
 */

#pragma once

#include <memory>
#include <optional>
#include <string>

#include "raft/util/types.h"

namespace raft {

/**
 * @brief Abstract interface for persisting currentTerm and votedFor.
 *
 * Thread safety: called only from the main Raft thread.
 */
class PersistentState {
   public:
    virtual ~PersistentState() = default;

    /**
     * @brief Read the persisted current term.
     *
     * Returns Term{0} if no state has been persisted yet (fresh node).
     */
    virtual Term current_term() const = 0;

    /**
     * @brief Read the persisted votedFor.
     *
     * Returns nullopt if the node has not voted in the current term.
     */
    virtual std::optional<NodeId> voted_for() const = 0;

    /**
     * @brief Atomically persist the new term and votedFor.
     *
     * MUST be durable (fsync'd) before this method returns.  The caller
     * relies on this for safety: if the process crashes immediately after,
     * the values must be recoverable.
     *
     * Preconditions:
     *   - term >= current_term()  (terms never decrease)
     *   - If term > current_term(), voted_for should typically be nullopt
     *     (vote is reset when moving to a new term).
     *   - If term == current_term(), voted_for is either the same as before
     *     or transitions from nullopt to a value (a node votes at most once
     *     per term).
     */
    virtual Result<void> save(Term term, std::optional<NodeId> voted_for) = 0;
};

/**
 * @brief Creates a file-backed PersistentState in the given directory.
 *
 * @param directory  Path to the directory for the state file.
 */
std::unique_ptr<PersistentState> create_file_persistent_state(const std::string& directory);

}  // namespace raft
