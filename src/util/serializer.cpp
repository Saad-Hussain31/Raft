/**
 * @file serializer.cpp
 * @brief JSON-based serializer using nlohmann/json.
 *
 * Wire format for each message:
 *   {
 *       "type": "RequestVote",
 *       "term": 5,
 *       "candidate_id": 2,
 *       ...
 *   }
 *
 * This is intentionally human-readable for development and debugging.
 * Replace with a binary format (protobuf, flatbuffers) for production throughput.
 */

#include "raft/util/serializer.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

namespace raft {

using json = nlohmann::json;

class JsonSerializer final : public Serializer {
   public:
    // ── Serialize ───────────────────────────────────────────────────────

    std::vector<uint8_t> serialize(const rpc::RequestVote& msg) override {
        // TODO: Build a JSON object with fields: type, term, candidate_id,
        //       last_log_index, last_log_term.
        //       Convert to string, then to vector<uint8_t>.
        (void)msg;
        throw std::logic_error("JsonSerializer::serialize(RequestVote) not implemented");
    }

    std::vector<uint8_t> serialize(const rpc::RequestVoteReply& msg) override {
        (void)msg;
        throw std::logic_error("JsonSerializer::serialize(RequestVoteReply) not implemented");
    }

    std::vector<uint8_t> serialize(const rpc::AppendEntries& msg) override {
        // TODO: Serialise all fields including the entries array.
        //       Each LogEntry in the array should be a nested JSON object
        //       with term, index, type, and base64-encoded data.
        (void)msg;
        throw std::logic_error("JsonSerializer::serialize(AppendEntries) not implemented");
    }

    std::vector<uint8_t> serialize(const rpc::AppendEntriesReply& msg) override {
        (void)msg;
        throw std::logic_error("JsonSerializer::serialize(AppendEntriesReply) not implemented");
    }

    std::vector<uint8_t> serialize(const rpc::InstallSnapshot& msg) override {
        // TODO: Include base64-encoded snapshot chunk data.
        (void)msg;
        throw std::logic_error("JsonSerializer::serialize(InstallSnapshot) not implemented");
    }

    std::vector<uint8_t> serialize(const rpc::InstallSnapshotReply& msg) override {
        (void)msg;
        throw std::logic_error("JsonSerializer::serialize(InstallSnapshotReply) not implemented");
    }

    std::vector<uint8_t> serialize(const rpc::ClientRequest& msg) override {
        (void)msg;
        throw std::logic_error("JsonSerializer::serialize(ClientRequest) not implemented");
    }

    std::vector<uint8_t> serialize(const rpc::ClientResponse& msg) override {
        (void)msg;
        throw std::logic_error("JsonSerializer::serialize(ClientResponse) not implemented");
    }

    // ── Deserialize ─────────────────────────────────────────────────────

    rpc::MessageType detect_type(std::span<const uint8_t> data) override {
        // TODO: Parse JSON, read the "type" field, and return the matching
        //       MessageType enum value.
        (void)data;
        throw std::logic_error("JsonSerializer::detect_type() not implemented");
    }

    rpc::RequestVote deserialize_request_vote(std::span<const uint8_t> data) override {
        (void)data;
        throw std::logic_error("Not implemented");
    }

    rpc::RequestVoteReply deserialize_request_vote_reply(std::span<const uint8_t> data) override {
        (void)data;
        throw std::logic_error("Not implemented");
    }

    rpc::AppendEntries deserialize_append_entries(std::span<const uint8_t> data) override {
        (void)data;
        throw std::logic_error("Not implemented");
    }

    rpc::AppendEntriesReply deserialize_append_entries_reply(
        std::span<const uint8_t> data) override {
        (void)data;
        throw std::logic_error("Not implemented");
    }

    rpc::InstallSnapshot deserialize_install_snapshot(std::span<const uint8_t> data) override {
        (void)data;
        throw std::logic_error("Not implemented");
    }

    rpc::InstallSnapshotReply deserialize_install_snapshot_reply(
        std::span<const uint8_t> data) override {
        (void)data;
        throw std::logic_error("Not implemented");
    }

    rpc::ClientRequest deserialize_client_request(std::span<const uint8_t> data) override {
        (void)data;
        throw std::logic_error("Not implemented");
    }

    rpc::ClientResponse deserialize_client_response(std::span<const uint8_t> data) override {
        (void)data;
        throw std::logic_error("Not implemented");
    }

    // ── Log entry ───────────────────────────────────────────────────────

    std::vector<uint8_t> serialize_entry(const LogEntry& entry) override {
        // TODO: Serialize a LogEntry to JSON bytes.
        (void)entry;
        throw std::logic_error("Not implemented");
    }

    LogEntry deserialize_entry(std::span<const uint8_t> data) override {
        (void)data;
        throw std::logic_error("Not implemented");
    }

    // ── Snapshot metadata ───────────────────────────────────────────────

    std::vector<uint8_t> serialize_snapshot_meta(const SnapshotMetadata& meta) override {
        (void)meta;
        throw std::logic_error("Not implemented");
    }

    SnapshotMetadata deserialize_snapshot_meta(std::span<const uint8_t> data) override {
        (void)data;
        throw std::logic_error("Not implemented");
    }
};

std::unique_ptr<Serializer> create_json_serializer() {
    return std::make_unique<JsonSerializer>();
}

}  // namespace raft
