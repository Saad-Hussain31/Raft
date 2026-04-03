/**
 * @file file_snapshot_store.cpp
 * @brief File-backed snapshot store implementation stubs.
 *
 * Storage layout:
 *   snapshots/
 *     current.meta     — JSON metadata (last_included_index, last_included_term, config)
 *     current.data     — Raw snapshot bytes
 *     tmp.meta         — In-progress snapshot metadata
 *     tmp.data         — In-progress snapshot data
 *
 * Atomicity:
 *   - New snapshots are written to tmp.meta + tmp.data.
 *   - finalize_snapshot() renames tmp → current (atomic on POSIX).
 *   - On startup, if tmp files exist, they are from an interrupted snapshot
 *     and should be deleted.
 */

#include <filesystem>
#include <fstream>

#include "raft/storage/snapshot_store.h"

namespace raft {

class FileSnapshotStore final : public SnapshotStore {
   public:
    explicit FileSnapshotStore(std::string directory) : directory_{std::move(directory)} {
        // TODO: Create the directory if it doesn't exist.
        // TODO: Clean up any leftover tmp files from a crash.
        // TODO: If current.meta exists, load it into cached_meta_.
    }

    std::optional<SnapshotMetadata> latest_metadata() const override {
        // TODO: Return cached_meta_.
        return std::nullopt;
    }

    std::vector<uint8_t> read_snapshot() const override {
        // TODO: Read and return the contents of current.data.
        return {};
    }

    std::vector<uint8_t> read_chunk(uint64_t offset, uint64_t length) const override {
        // TODO: Open current.data, seek to offset, read up to length bytes.
        return {};
    }

    uint64_t snapshot_size() const override {
        // TODO: Return the file size of current.data, or 0 if it doesn't exist.
        return 0;
    }

    Result<void> begin_snapshot(const SnapshotMetadata& meta) override {
        // TODO: Write meta to tmp.meta.
        //       Open tmp.data for writing (truncate if exists).
        //       Store meta in pending_meta_.
        return Result<void>::failure(ErrorCode::NotImplemented);
    }

    Result<void> write_chunk(uint64_t offset, std::span<const uint8_t> data) override {
        // TODO: Seek to offset in tmp.data and write data.
        return Result<void>::failure(ErrorCode::NotImplemented);
    }

    Result<void> finalize_snapshot() override {
        // TODO: fsync tmp.data and tmp.meta.
        //       Rename tmp.meta → current.meta.
        //       Rename tmp.data → current.data.
        //       Update cached_meta_ = pending_meta_.
        return Result<void>::failure(ErrorCode::NotImplemented);
    }

    void abort_snapshot() override {
        // TODO: Delete tmp.meta and tmp.data if they exist.
    }

   private:
    std::string directory_;
    std::optional<SnapshotMetadata> cached_meta_;
    std::optional<SnapshotMetadata> pending_meta_;
};

std::unique_ptr<SnapshotStore> create_file_snapshot_store(const std::string& directory) {
    return std::make_unique<FileSnapshotStore>(directory);
}

}  // namespace raft
