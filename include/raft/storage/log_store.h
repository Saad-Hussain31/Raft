/**
 * @file log_store.h
 * @brief Persistent log storage interface.
 *
 * The LogStore is responsible for durably storing log entries to disk.  The Raft
 * safety guarantee depends on entries being persisted BEFORE an AppendEntries
 * reply is sent — the implementation MUST honour this.
 *
 * A correct LogStore must satisfy:
 *   - Entries are stored in index order.
 *   - Once an entry is persisted and sync'd, it survives crash and restart.
 *   - Truncation removes entries from a given index onward.
 *   - The store can report its first and last index (for bootstrapping the
 *     in-memory log after restart).
 *
 * The default implementation provided writes entries to a simple append-only
 * file.  A production system may use RocksDB, SQLite, or a WAL.
 */

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "raft/util/types.h"

namespace raft {

/**
 * @brief Abstract interface for persistent log entry storage.
 *
 * Thread safety: methods are called from the main Raft thread.  Implementations
 * do NOT need to be thread-safe unless the caller explicitly synchronizes.
 */
class LogStore {
   public:
    virtual ~LogStore() = default;

    /**
     * @brief Return the index of the first entry still present in the store.
     *
     * After log compaction / snapshotting, early entries are removed.
     * Returns LogIndex::none() if the store is empty.
     */
    virtual LogIndex first_index() const = 0;

    /**
     * @brief Return the index of the last entry in the store.
     *
     * Returns LogIndex::none() if the store is empty.
     */
    virtual LogIndex last_index() const = 0;

    /**
     * @brief Retrieve a single entry by index.
     *
     * @return The entry, or std::nullopt if the index is out of range.
     */
    virtual std::optional<LogEntry> entry_at(LogIndex index) const = 0;

    /**
     * @brief Retrieve a range of entries [from, to] inclusive.
     *
     * Used by the leader when building AppendEntries RPCs and for log
     * replication catch-up.
     */
    virtual std::vector<LogEntry> entries(LogIndex from, LogIndex to) const = 0;

    /**
     * @brief Persist one or more entries.
     *
     * The entries MUST have contiguous indexes starting at last_index() + 1.
     * After this call returns, the entries must survive a crash (i.e. they
     * must be fsync'd or equivalent).
     *
     * @return Ok on success, IoError on disk failure.
     */
    virtual Result<void> append(const std::vector<LogEntry>& entries) = 0;

    /**
     * @brief Remove all entries from @p from_index onward (inclusive).
     *
     * Used when a follower finds a conflicting entry and must discard the
     * tail of its log.  Must be durable.
     *
     * Precondition: from_index >= first_index().
     */
    virtual Result<void> truncate_from(LogIndex from_index) = 0;

    /**
     * @brief Remove all entries before @p up_to_index (exclusive).
     *
     * Used after a snapshot is taken — entries included in the snapshot can
     * be discarded to reclaim disk space.
     *
     * Precondition: up_to_index <= last_index() + 1.
     */
    virtual Result<void> truncate_before(LogIndex up_to_index) = 0;

    /**
     * @brief Force any buffered writes to durable storage.
     */
    virtual Result<void> sync() = 0;
};

/**
 * @brief Creates a file-backed LogStore that writes to the given directory.
 *
 * @param directory  Path to the directory for log files.
 */
std::unique_ptr<LogStore> create_file_log_store(const std::string& directory);

}  // namespace raft
