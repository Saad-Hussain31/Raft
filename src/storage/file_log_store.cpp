/**
 * @file file_log_store.cpp
 * @brief File-backed log store implementation stubs.
 *
 * Storage format:
 *   - Entries are stored in an append-only binary file.
 *   - Each entry is serialized as:  [4 bytes entry length][serialized entry bytes]
 *   - An index file maps LogIndex → byte offset for O(1) random access.
 *   - On startup, the index file is rebuilt if it is missing or incomplete.
 *   - Writes are fsync'd before returning for durability.
 *   - Truncation from a given index rewrites the index file and truncates
 *     the data file.
 */

#include <filesystem>
#include <fstream>

#include "raft/storage/log_store.h"

namespace raft {

class FileLogStore final : public LogStore {
   public:
    explicit FileLogStore(std::string directory) : directory_{std::move(directory)} {
        // TODO: Create the directory if it doesn't exist.
        // TODO: Open or create the data file and index file.
        // TODO: If the index file exists, load it to populate index_.
        // TODO: If only the data file exists, rebuild the index by scanning.
    }

    LogIndex first_index() const override {
        // TODO: Return the first index in the index map, or LogIndex::none().
        return LogIndex::none();
    }

    LogIndex last_index() const override {
        // TODO: Return the last index in the index map, or LogIndex::none().
        return LogIndex::none();
    }

    std::optional<LogEntry> entry_at(LogIndex index) const override {
        // TODO: Look up the byte offset for `index` in the index map.
        //       Seek to that offset in the data file and deserialize one entry.
        return std::nullopt;
    }

    std::vector<LogEntry> entries(LogIndex from, LogIndex to) const override {
        // TODO: Read and deserialize entries in [from, to].
        return {};
    }

    Result<void> append(const std::vector<LogEntry>& entries) override {
        // TODO: Serialize each entry and append to the data file.
        //       Update the index map with the byte offset for each entry.
        //       fsync the data file.
        //       fsync the index file.
        return Result<void>::failure(ErrorCode::NotImplemented);
    }

    Result<void> truncate_from(LogIndex from_index) override {
        // TODO: Remove entries from from_index onward.
        //       Truncate the data file at the byte offset of from_index.
        //       Remove entries from the index map.
        //       Rewrite and fsync the index file.
        return Result<void>::failure(ErrorCode::NotImplemented);
    }

    Result<void> truncate_before(LogIndex up_to_index) override {
        // TODO: Remove entries before up_to_index.
        //       This is more complex: either rewrite the data file without
        //       early entries, or maintain a "start offset" and only compact
        //       occasionally.
        return Result<void>::failure(ErrorCode::NotImplemented);
    }

    Result<void> sync() override {
        // TODO: fsync both the data file and index file.
        return Result<void>::failure(ErrorCode::NotImplemented);
    }

   private:
    std::string directory_;
    // TODO: Member variables:
    //   - Data file handle (std::fstream or fd).
    //   - Index map: std::map<LogIndex, uint64_t> (index → byte offset).
    //   - A Serializer instance for encoding/decoding entries.
};

std::unique_ptr<LogStore> create_file_log_store(const std::string& directory) {
    return std::make_unique<FileLogStore>(directory);
}

}  // namespace raft
