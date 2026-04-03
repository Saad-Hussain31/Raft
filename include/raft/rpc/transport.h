/**
 * @file transport.h
 * @brief Network transport interface for Raft RPC communication.
 *
 * The transport layer is responsible for sending and receiving RPC messages
 * between Raft nodes.  It is completely decoupled from the consensus logic:
 * the RaftNode submits messages to the transport, and the transport delivers
 * incoming messages to a callback.
 *
 * Design requirements:
 *   - Must support point-to-point request/response messaging.
 *   - Must handle disconnects, retries, and timeouts gracefully.
 *   - Must support running multiple nodes on the same machine (different ports).
 *   - Connection management (connect, reconnect, backoff) is internal.
 *   - Message framing: 4-byte length prefix + payload.
 *   - Must log network failures for debugging without crashing.
 *
 * The concrete TcpTransport implementation uses POSIX sockets with non-blocking
 * I/O and epoll/poll for event-driven multiplexing.
 */

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "raft/rpc/messages.h"
#include "raft/util/logger.h"
#include "raft/util/types.h"

namespace raft {

/**
 * @brief Callback invoked when an RPC message is received from a peer.
 *
 * @param from      The ID of the sending node.
 * @param type      The message type (for dispatch).
 * @param payload   The raw serialized message bytes (excluding length prefix).
 *
 * The transport does NOT deserialize messages — that is the RaftNode's
 * responsibility, using the Serializer.
 */
using MessageHandler =
    std::function<void(NodeId from, rpc::MessageType type, std::vector<uint8_t> payload)>;

/**
 * @brief Abstract transport interface.
 */
class Transport {
   public:
    virtual ~Transport() = default;

    /**
     * @brief Start listening for incoming connections.
     *
     * @param local_id   This node's ID (for identification in the protocol).
     * @param address    The address:port to bind and listen on.
     * @param handler    Callback for incoming messages.
     *
     * This method starts background threads or an event loop.
     * It returns immediately after binding.
     *
     * @return Ok on success; IoError if the port is unavailable.
     */
    virtual Result<void> start(NodeId local_id, const PeerAddress& address,
                               MessageHandler handler) = 0;

    /**
     * @brief Stop the transport: close all connections, stop threads.
     *
     * After this call returns, no more callbacks will be invoked.
     * Outstanding sends may be dropped.
     */
    virtual void stop() = 0;

    /**
     * @brief Register a peer that this node may communicate with.
     *
     * The transport will attempt to establish and maintain a connection to
     * this peer.  If the connection drops, it should reconnect with
     * exponential backoff.
     *
     * @param peer_id   The peer's node ID.
     * @param address   The peer's network endpoint.
     */
    virtual void add_peer(NodeId peer_id, const PeerAddress& address) = 0;

    /**
     * @brief Remove a peer (e.g. after a configuration change).
     *
     * Closes any connection to this peer and stops reconnection attempts.
     */
    virtual void remove_peer(NodeId peer_id) = 0;

    /**
     * @brief Send a serialized message to a specific peer.
     *
     * The transport handles framing (length prefix) and connection management.
     * If the peer is not currently connected, the message may be queued
     * briefly or dropped.  The send is asynchronous — this method returns
     * immediately.
     *
     * @param to        The destination node's ID.
     * @param type      The message type tag.
     * @param payload   The serialized message bytes.
     *
     * @return Ok if the message was accepted for sending (NOT a delivery
     *         guarantee).  IoError if the peer is unknown.
     */
    virtual Result<void> send(NodeId to, rpc::MessageType type, std::vector<uint8_t> payload) = 0;

    /**
     * @brief Send a serialized message to all known peers.
     *
     * Convenience method for leader broadcasts (AppendEntries, etc.).
     * Equivalent to iterating over all peers and calling send() on each.
     */
    virtual void broadcast(rpc::MessageType type, std::vector<uint8_t> payload) = 0;
};

/**
 * @brief Creates a TCP-based transport.
 *
 * Uses POSIX sockets with non-blocking I/O.  Suitable for running multiple
 * Raft nodes as separate processes on a single machine.
 *
 * @param logger      Logger for network diagnostics.
 * @param serializer  Not used directly (transport is payload-agnostic), but
 *                    available for message-type detection in framing.
 */
std::unique_ptr<Transport> create_tcp_transport(std::shared_ptr<Logger> logger);

}  // namespace raft
