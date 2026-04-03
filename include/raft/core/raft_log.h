/**
 * @file raft_log.h
 * @brief In-memory log management layered on top of LogStore.
 *
 * RaftLog is the bridge between the in-memory consensus logic and the
 * persistent LogStore.  It maintains:
 *   - An in-memory cache of recent log entries for fast access.
 *   - Snapshot metadata (the last compacted index/term).
 *   - The relationship between logical indexes and physical storage.
 *
 * After a snapshot, entries before last_included_index are discarded from
 * memory and from the LogStore.  The RaftLog must handle the offset correctly
 * so that external callers always use absolute indexes.
 *
 * Thread safety: accessed only from the main Raft thread.
 */

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "raft/storage/log_store.h"
#include "raft/util/types.h"

namespace raft {

class RaftLog {
   public:
    /**
     * @brief Construct a RaftLog backed by the given persistent store.
     *
     * On construction, the log loads metadata from the store to reconstruct
     * the in-memory index range.
     */
    explicit RaftLog(std::unique_ptr<LogStore> store);

    // ── Queries ─────────────────────────────────────────────────────────

    /**
     * @brief The index of the last entry in the log (or the snapshot's
     *        last_included_index if the log is empty after compaction).
     */
    LogIndex last_index() const;

    /**
     * @brief The term of the last entry in the log.
     */
    Term last_term() const;

    /**
     * @brief Retrieve the entry at the given index.
     *
     * @return The entry, or nullopt if the index is out of range (either
     *         before the snapshot or after last_index()).
     */
    std::optional<LogEntry> entry_at(LogIndex index) const;

    /**
     * @brief Retrieve the term of the entry at the given index.
     *
     * Returns Term{0} if the index is LogIndex::none() (the sentinel, used
     * when prev_log_index is 0 for the first entry).
     * Returns the snapshot term if the index equals last_included_index.
     * Returns nullopt if the index is out of range.
     */
    std::optional<Term> term_at(LogIndex index) const;

    /**
     * @brief Retrieve entries in [from, to] inclusive.
     *
     * Used by the leader to build AppendEntries batches.
     */
    std::vector<LogEntry> slice(LogIndex from, LogIndex to) const;

    /**
     * @brief Check if a (index, term) pair matches an entry in the log.
     *
     * This is the log matching property (§5.3):
     *   "If two entries in different logs have the same index and term,
     *    then the logs are identical in all preceding entries."
     *
     * @param index  The index to check.
     * @param term   The term to compare against.
     * @return true if the log contains an entry at @p index with term @p term,
     *         OR if index is 0 (the implicit sentinel).
     */
    bool match_term(LogIndex index, Term term) const;

    /** @brief Number of entries currently in the log (excluding snapshot). */
    uint64_t size() const;

    // ── Mutations ───────────────────────────────────────────────────────

    /**
     * @brief Append a new entry to the log (persisted to the store).
     *
     * The entry's index is set to last_index() + 1.
     *
     * @return The index assigned to the entry, or an error.
     */
    Result<LogIndex> append(LogEntry entry);

    /**
     * @brief Append multiple entries.
     *
     * Entries must have contiguous indexes starting at last_index() + 1.
     */
    Result<void> append_entries(std::vector<LogEntry> entries);

    /**
     * @brief Delete all entries from @p from_index onward.
     *
     * Used by followers when a conflicting entry is detected.
     *
     * Invariant: from_index > snapshot's last_included_index (cannot
     * truncate entries already captured in a snapshot).
     */
    Result<void> truncate_from(LogIndex from_index);

    // ── Snapshot support ────────────────────────────────────────────────

    /**
     * @brief Record that a snapshot has been taken up through the given
     *        index and term, then compact the log.
     *
     * 1. Set snapshot metadata.
     * 2. Remove all entries with index <= @p last_included_index from the
     *    in-memory cache and the persistent store.
     *
     * Precondition: last_included_index <= last_index().
     */
    Result<void> compact(LogIndex last_included_index, Term last_included_term);

    /**
     * @brief Apply a snapshot received via InstallSnapshot.
     *
     * If last_included_index covers a prefix of the current log, discard
     * covered entries and keep the rest.  If it extends beyond the log,
     * replace the entire log.
     */
    Result<void> apply_snapshot(LogIndex last_included_index, Term last_included_term);

    /** @brief Snapshot metadata (if a snapshot has been taken). */
    std::optional<SnapshotMetadata> snapshot_metadata() const;

   private:
    std::unique_ptr<LogStore> store_;
    std::optional<SnapshotMetadata> snapshot_meta_;

    /// In-memory cache of entries from (snapshot_last_index, last_index].
    std::vector<LogEntry> cache_;
};

}  // namespace raft
