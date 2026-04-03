/**
 * @file types.h
 * @brief Strong type definitions for the Raft protocol.
 *
 * This file defines strongly-typed wrappers for all fundamental Raft concepts.
 * Strong types prevent accidental mixing of terms, indexes, and IDs at compile
 * time.  Every integer that carries domain meaning gets its own type.
 *
 * Design rationale:
 *   - A Term is NOT an index; an index is NOT a node ID.  Using raw integers
 *     for all of them invites subtle bugs that only surface under concurrency.
 *   - Comparison, arithmetic, and hashing are provided through a CRTP base so
 *     the wrappers are zero-cost in release builds.
 */

#pragma once

#include <compare>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

namespace raft {

// ─── CRTP base for strongly-typed integers ──────────────────────────────────

/**
 * @brief Zero-cost wrapper that gives a raw integer a unique type.
 *
 * @tparam Derived  The concrete strong type (e.g. Term, LogIndex).
 * @tparam T        The underlying arithmetic type (default: uint64_t).
 */
template <typename Derived, typename T = uint64_t>
struct StrongInteger {
    T value{};

    constexpr StrongInteger() = default;
    constexpr explicit StrongInteger(T v) : value{v} {}

    constexpr auto operator<=>(const StrongInteger&) const = default;
    constexpr bool operator==(const StrongInteger&) const = default;

    constexpr Derived& operator++() {
        ++value;
        return static_cast<Derived&>(*this);
    }
    constexpr Derived operator++(int) {
        auto copy = static_cast<Derived&>(*this);
        ++value;
        return copy;
    }
    constexpr Derived& operator--() {
        --value;
        return static_cast<Derived&>(*this);
    }
    constexpr Derived operator+(const Derived& rhs) const { return Derived{value + rhs.value}; }
    constexpr Derived operator-(const Derived& rhs) const { return Derived{value - rhs.value}; }

    friend std::ostream& operator<<(std::ostream& os, const StrongInteger& s) {
        return os << s.value;
    }
};

// ─── Core Raft types ────────────────────────────────────────────────────────

/**
 * @brief Monotonically increasing term number.
 *
 * Per the Raft paper, terms act as a logical clock.  Each term begins with an
 * election.  Terms are compared to detect stale leaders and prevent split-brain.
 * A server must NEVER decrease its current term.
 */
struct Term : StrongInteger<Term> {
    using StrongInteger::StrongInteger;
};

/**
 * @brief One-based index into the replicated log.
 *
 * Index 0 is reserved as a sentinel ("no entry").  The first real entry has
 * index 1.  Log indexes are used for commit tracking, log matching, and
 * snapshotting.
 */
struct LogIndex : StrongInteger<LogIndex> {
    using StrongInteger::StrongInteger;

    /** @brief Sentinel value representing "no index". */
    static constexpr LogIndex none() { return LogIndex{0}; }
};

/**
 * @brief Unique identifier for a node in the Raft cluster.
 *
 * Node IDs are persistent across restarts.  They must be unique within a
 * cluster and stable across configuration changes.
 */
struct NodeId : StrongInteger<NodeId> {
    using StrongInteger::StrongInteger;
};

/**
 * @brief Unique identifier for a Raft cluster.
 *
 * Prevents accidentally joining the wrong cluster after a misconfiguration.
 */
struct ClusterId {
    std::string value;

    bool operator==(const ClusterId&) const = default;
    auto operator<=>(const ClusterId&) const = default;
};

// ─── Raft server role ───────────────────────────────────────────────────────

/**
 * @brief The three roles a Raft server can hold.
 *
 * A server is in exactly one of these states at any time.  Transitions:
 *   Follower → Candidate  (election timeout fires without hearing from leader)
 *   Candidate → Leader    (wins election)
 *   Candidate → Follower  (discovers higher term or new leader)
 *   Leader → Follower     (discovers higher term)
 */
enum class Role : uint8_t {
    Follower,
    Candidate,
    Leader,
};

inline std::string_view to_string(Role r) {
    switch (r) {
        case Role::Follower:
            return "Follower";
        case Role::Candidate:
            return "Candidate";
        case Role::Leader:
            return "Leader";
    }
    return "Unknown";
}

// ─── Log entry ──────────────────────────────────────────────────────────────

/**
 * @brief Type tag for log entries.
 *
 * Noop entries are appended by a new leader to commit entries from previous
 * terms (§5.4.2 of the Raft paper).  Config entries carry cluster membership
 * changes and trigger joint-consensus transitions.
 */
enum class EntryType : uint8_t {
    Command,  ///< Normal client command — applied to the state machine.
    Noop,     ///< Leader-appended no-op for term commitment.
    Config,   ///< Cluster configuration change (membership).
};

/**
 * @brief A single entry in the replicated log.
 *
 * Invariants:
 *   - term ≥ 1  (term 0 is invalid)
 *   - index ≥ 1 (index 0 is the sentinel)
 *   - data is opaque to the consensus module; only the state machine
 *     interprets it.
 *   - For Config entries, data contains the serialized new configuration.
 *
 * The log MUST persist entries to stable storage before responding to an
 * AppendEntries RPC.
 */
struct LogEntry {
    Term term{};
    LogIndex index{};
    EntryType type{EntryType::Command};
    std::vector<uint8_t> data;
};

// ─── Snapshot metadata ──────────────────────────────────────────────────────

/**
 * @brief Metadata associated with a snapshot.
 *
 * A snapshot captures the state machine up through (lastIncludedIndex,
 * lastIncludedTerm).  All log entries up to and including lastIncludedIndex
 * can be discarded after a snapshot is taken.
 *
 * The configuration field records the cluster membership as of the snapshot,
 * which is needed for safe restart (the node must know the cluster peers even
 * if the config entry has been compacted away).
 */
struct SnapshotMetadata {
    LogIndex last_included_index{};
    Term last_included_term{};
    std::vector<uint8_t> configuration;  ///< Serialized cluster config at snapshot time.
};

// ─── Network address ────────────────────────────────────────────────────────

/**
 * @brief A host:port pair representing a peer's network endpoint.
 */
struct PeerAddress {
    std::string host;
    uint16_t port{0};

    bool operator==(const PeerAddress&) const = default;

    std::string to_string() const { return host + ":" + std::to_string(port); }
};

// ─── Error handling ─────────────────────────────────────────────────────────

/**
 * @brief Error codes for operations that can fail.
 */
enum class ErrorCode : uint8_t {
    Ok,
    NotLeader,
    Timeout,
    ShuttingDown,
    InvalidArgument,
    IoError,
    SnapshotInProgress,
    NotImplemented,
};

inline std::string_view to_string(ErrorCode e) {
    switch (e) {
        case ErrorCode::Ok:
            return "Ok";
        case ErrorCode::NotLeader:
            return "NotLeader";
        case ErrorCode::Timeout:
            return "Timeout";
        case ErrorCode::ShuttingDown:
            return "ShuttingDown";
        case ErrorCode::InvalidArgument:
            return "InvalidArgument";
        case ErrorCode::IoError:
            return "IoError";
        case ErrorCode::SnapshotInProgress:
            return "SnapshotInProgress";
        case ErrorCode::NotImplemented:
            return "NotImplemented";
    }
    return "Unknown";
}

/**
 * @brief Lightweight result type for fallible operations.
 *
 * Carries either a success value or an error code.
 * Used throughout the codebase instead of exceptions for predictable control flow.
 */
template <typename T>
struct Result {
    ErrorCode error{ErrorCode::Ok};
    T value{};

    bool ok() const { return error == ErrorCode::Ok; }

    static Result success(T val) { return {ErrorCode::Ok, std::move(val)}; }
    static Result failure(ErrorCode ec) { return {ec, {}}; }
};

/// Specialization for void-result operations.
template <>
struct Result<void> {
    ErrorCode error{ErrorCode::Ok};

    bool ok() const { return error == ErrorCode::Ok; }

    static Result success() { return {ErrorCode::Ok}; }
    static Result failure(ErrorCode ec) { return {ec}; }
};

}  // namespace raft

// ─── std::hash specializations ──────────────────────────────────────────────

template <>
struct std::hash<raft::NodeId> {
    size_t operator()(const raft::NodeId& id) const noexcept {
        return std::hash<uint64_t>{}(id.value);
    }
};

template <>
struct std::hash<raft::Term> {
    size_t operator()(const raft::Term& t) const noexcept { return std::hash<uint64_t>{}(t.value); }
};

template <>
struct std::hash<raft::LogIndex> {
    size_t operator()(const raft::LogIndex& i) const noexcept {
        return std::hash<uint64_t>{}(i.value);
    }
};
