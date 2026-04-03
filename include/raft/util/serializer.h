/**
 * @file serializer.h
 * @brief Serialization and deserialization for Raft RPC messages.
 *
 * The serializer is responsible for converting RPC message structs to and from
 * a byte representation suitable for network transmission and persistent
 * storage.  The transport layer calls serialize() before sending and
 * deserialize() after receiving.
 *
 * Design decisions:
 *   - Uses nlohmann/json as the wire format for readability and debuggability
 *     during development.  A production system can swap in Protocol Buffers or
 *     FlatBuffers by implementing the same interface.
 *   - All messages are length-prefixed on the wire:
 *       [4 bytes: message length (network byte order)][JSON payload]
 *   - The message type tag is included in the JSON so the receiver can
 *     dispatch without out-of-band information.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "raft/rpc/messages.h"
#include "raft/util/types.h"

namespace raft {

/**
 * @brief Abstract serializer interface.
 *
 * Implementations must be stateless and thread-safe.
 */
class Serializer {
   public:
    virtual ~Serializer() = default;

    // ── Serialize ───────────────────────────────────────────────────────
    virtual std::vector<uint8_t> serialize(const rpc::RequestVote& msg) = 0;
    virtual std::vector<uint8_t> serialize(const rpc::RequestVoteReply& msg) = 0;
    virtual std::vector<uint8_t> serialize(const rpc::AppendEntries& msg) = 0;
    virtual std::vector<uint8_t> serialize(const rpc::AppendEntriesReply& msg) = 0;
    virtual std::vector<uint8_t> serialize(const rpc::InstallSnapshot& msg) = 0;
    virtual std::vector<uint8_t> serialize(const rpc::InstallSnapshotReply& msg) = 0;
    virtual std::vector<uint8_t> serialize(const rpc::ClientRequest& msg) = 0;
    virtual std::vector<uint8_t> serialize(const rpc::ClientResponse& msg) = 0;

    // ── Deserialize ─────────────────────────────────────────────────────

    /**
     * @brief Detect the message type from a serialized payload.
     *
     * @param data  The raw bytes (excluding the 4-byte length prefix).
     * @return The message type, allowing the caller to invoke the correct
     *         typed deserialize method.
     */
    virtual rpc::MessageType detect_type(std::span<const uint8_t> data) = 0;

    virtual rpc::RequestVote deserialize_request_vote(std::span<const uint8_t> data) = 0;
    virtual rpc::RequestVoteReply deserialize_request_vote_reply(std::span<const uint8_t> data) = 0;
    virtual rpc::AppendEntries deserialize_append_entries(std::span<const uint8_t> data) = 0;
    virtual rpc::AppendEntriesReply deserialize_append_entries_reply(
        std::span<const uint8_t> data) = 0;
    virtual rpc::InstallSnapshot deserialize_install_snapshot(std::span<const uint8_t> data) = 0;
    virtual rpc::InstallSnapshotReply deserialize_install_snapshot_reply(
        std::span<const uint8_t> data) = 0;
    virtual rpc::ClientRequest deserialize_client_request(std::span<const uint8_t> data) = 0;
    virtual rpc::ClientResponse deserialize_client_response(std::span<const uint8_t> data) = 0;

    // ── Log entry serialization (for persistent storage) ────────────────
    virtual std::vector<uint8_t> serialize_entry(const LogEntry& entry) = 0;
    virtual LogEntry deserialize_entry(std::span<const uint8_t> data) = 0;

    // ── Snapshot metadata serialization ─────────────────────────────────
    virtual std::vector<uint8_t> serialize_snapshot_meta(const SnapshotMetadata& meta) = 0;
    virtual SnapshotMetadata deserialize_snapshot_meta(std::span<const uint8_t> data) = 0;
};

/**
 * @brief Creates a JSON-based serializer using nlohmann/json.
 */
std::unique_ptr<Serializer> create_json_serializer();

}  // namespace raft
