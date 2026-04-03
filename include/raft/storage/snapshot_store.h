/**
 * @file snapshot_store.h
 * @brief Persistent snapshot storage interface.
 *
 * Snapshots capture the entire state machine at a point in time, identified by
 * (last_included_index, last_included_term).  After a snapshot is taken, all
 * log entries up through last_included_index can be discarded.
 *
 * The SnapshotStore is also used during InstallSnapshot RPCs: the leader
 * streams snapshot chunks to a follower, which writes them here.
 *
 * Invariants:
 *   - At most one snapshot is "current" at any time.
 *   - A new snapshot replaces the old one atomically (write to temp, rename).
 *   - The metadata file must be kept consistent with the data file.
 *   - On crash recovery, the store must be able to detect and discard partial
 *     (incomplete) snapshots.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "raft/util/types.h"

namespace raft {

/**
 * @brief Abstract interface for persistent snapshot storage.
 */
class SnapshotStore {
   public:
    virtual ~SnapshotStore() = default;

    /**
     * @brief Returns metadata for the most recent snapshot, or nullopt if
     *        no snapshot has been taken.
     */
    virtual std::optional<SnapshotMetadata> latest_metadata() const = 0;

    /**
     * @brief Read the full snapshot data into memory.
     *
     * For large snapshots, consider a streaming variant.  This simple
     * interface is sufficient for the scaffold.
     *
     * @return The snapshot bytes, or an empty vector if no snapshot exists.
     */
    virtual std::vector<uint8_t> read_snapshot() const = 0;

    /**
     * @brief Read a chunk of the snapshot starting at @p offset.
     *
     * Used by the leader when sending InstallSnapshot RPCs in chunks.
     *
     * @param offset  Byte offset into the snapshot data.
     * @param length  Maximum number of bytes to return.
     * @return The chunk, possibly shorter than @p length if near EOF.
     */
    virtual std::vector<uint8_t> read_chunk(uint64_t offset, uint64_t length) const = 0;

    /**
     * @brief Total size in bytes of the current snapshot data.
     */
    virtual uint64_t snapshot_size() const = 0;

    /**
     * @brief Begin writing a new snapshot.
     *
     * Opens a temporary file for the new snapshot.  write_chunk() appends
     * data to this file.  finalize_snapshot() atomically replaces the old
     * snapshot with the new one.
     *
     * @param meta  Metadata for the snapshot being created.
     */
    virtual Result<void> begin_snapshot(const SnapshotMetadata& meta) = 0;

    /**
     * @brief Append a chunk of data at the given offset.
     *
     * @param offset  Byte offset in the snapshot being written.
     * @param data    The chunk bytes.
     */
    virtual Result<void> write_chunk(uint64_t offset, std::span<const uint8_t> data) = 0;

    /**
     * @brief Finalize the snapshot: sync, then atomically replace the old one.
     *
     * After this call, latest_metadata() must reflect the new snapshot and
     * read_snapshot() must return the new data.
     */
    virtual Result<void> finalize_snapshot() = 0;

    /**
     * @brief Abort an in-progress snapshot write, cleaning up temp files.
     */
    virtual void abort_snapshot() = 0;
};

/**
 * @brief Creates a file-backed SnapshotStore in the given directory.
 *
 * @param directory  Path to the directory for snapshot files.
 */
std::unique_ptr<SnapshotStore> create_file_snapshot_store(const std::string& directory);

}  // namespace raft
