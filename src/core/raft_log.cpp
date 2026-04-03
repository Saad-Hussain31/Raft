/**
 * @file raft_log.cpp
 * @brief In-memory log implementation stubs.
 */

#include "raft/core/raft_log.h"

#include <stdexcept>

namespace raft {

RaftLog::RaftLog(std::unique_ptr<LogStore> store) : store_{std::move(store)} {
    // TODO: On construction, load existing entries from the store into cache_.
    //
    //   1. Query store_->first_index() and store_->last_index().
    //   2. If the store is non-empty, load all entries into cache_.
    //   3. If a snapshot was previously taken, the store's first_index may
    //      be > 1.  The snapshot metadata must be set separately via
    //      apply_snapshot() or compact() before full operation.
}

// ── Queries ─────────────────────────────────────────────────────────────────

LogIndex RaftLog::last_index() const {
    // TODO: Return the index of the last entry.
    //
    // If cache_ is non-empty, return cache_.back().index.
    // Else if a snapshot exists, return snapshot_meta_->last_included_index.
    // Else return LogIndex::none().
    return LogIndex::none();
}

Term RaftLog::last_term() const {
    // TODO: Return the term of the last entry.
    //
    // If cache_ is non-empty, return cache_.back().term.
    // Else if a snapshot exists, return snapshot_meta_->last_included_term.
    // Else return Term{0}.
    return Term{0};
}

std::optional<LogEntry> RaftLog::entry_at(LogIndex index) const {
    // TODO: Retrieve entry at the given absolute index.
    //
    // If index falls within the cache:
    //   Compute offset = index - first_cached_index.
    //   Return cache_[offset].
    // If index == snapshot_meta_->last_included_index:
    //   Cannot return the entry (it was compacted).  Return nullopt.
    // If index < first_cached_index or index > last_index():
    //   Return nullopt.
    return std::nullopt;
}

std::optional<Term> RaftLog::term_at(LogIndex index) const {
    // TODO: Return the term of the entry at the given index.
    //
    // Special cases:
    //   - index == LogIndex::none() (0): return Term{0} (sentinel).
    //   - index == snapshot_meta_->last_included_index:
    //       return snapshot_meta_->last_included_term.
    //   - Otherwise, look up in cache.
    //
    // This is used for the log matching check in AppendEntries.
    if (index == LogIndex::none())
        return Term{0};
    return std::nullopt;
}

std::vector<LogEntry> RaftLog::slice(LogIndex from, LogIndex to) const {
    // TODO: Return entries in [from, to] inclusive.
    //
    // Bounds check: from >= first cached index, to <= last_index().
    // Compute offsets into cache_ and return a sub-vector.
    return {};
}

bool RaftLog::match_term(LogIndex index, Term term) const {
    // TODO: Check the log matching property.
    //
    // If index == LogIndex::none() and term == Term{0}: return true (sentinel).
    // Otherwise: return term_at(index) == term.
    if (index == LogIndex::none())
        return term == Term{0};
    auto t = term_at(index);
    return t.has_value() && *t == term;
}

uint64_t RaftLog::size() const {
    return cache_.size();
}

// ── Mutations ───────────────────────────────────────────────────────────────

Result<LogIndex> RaftLog::append(LogEntry entry) {
    // TODO: Append a single entry.
    //
    //   1. Set entry.index = last_index() + LogIndex{1}.
    //   2. Persist via store_->append({entry}).
    //   3. Add to cache_.
    //   4. Return the assigned index.
    return Result<LogIndex>::failure(ErrorCode::NotImplemented);
}

Result<void> RaftLog::append_entries(std::vector<LogEntry> entries) {
    // TODO: Append multiple entries.
    //
    //   1. Assign indexes starting at last_index() + 1.
    //   2. Persist via store_->append(entries).
    //   3. Add to cache_.
    return Result<void>::failure(ErrorCode::NotImplemented);
}

Result<void> RaftLog::truncate_from(LogIndex from_index) {
    // TODO: Remove entries from from_index onward.
    //
    //   1. Assert from_index > snapshot last_included_index.
    //   2. Persist via store_->truncate_from(from_index).
    //   3. Remove from cache_.
    return Result<void>::failure(ErrorCode::NotImplemented);
}

// ── Snapshot support ────────────────────────────────────────────────────────

Result<void> RaftLog::compact(LogIndex last_included_index, Term last_included_term) {
    // TODO: Discard entries up through last_included_index.
    //
    //   1. Update snapshot_meta_.
    //   2. Remove entries from cache_ where index <= last_included_index.
    //   3. Persist via store_->truncate_before(last_included_index + 1).
    return Result<void>::failure(ErrorCode::NotImplemented);
}

Result<void> RaftLog::apply_snapshot(LogIndex last_included_index, Term last_included_term) {
    // TODO: Handle an incoming snapshot.
    //
    //   1. Update snapshot_meta_.
    //   2. If the log has entries beyond last_included_index, keep them.
    //      Otherwise, clear the entire cache.
    //   3. Persist the truncation to the store.
    return Result<void>::failure(ErrorCode::NotImplemented);
}

std::optional<SnapshotMetadata> RaftLog::snapshot_metadata() const {
    return snapshot_meta_;
}

}  // namespace raft
