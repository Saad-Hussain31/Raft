/**
 * @file logger.h
 * @brief Abstract logging interface for the Raft implementation.
 *
 * The Raft library logs through this interface so the application can plug in
 * any backend (spdlog, stdout, file, systemd journal, etc.).
 *
 * A concrete spdlog-based implementation is provided in src/util/logger.cpp.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "raft/util/types.h"

namespace raft {

/**
 * @brief Severity levels, ordered from most to least verbose.
 */
enum class LogLevel : uint8_t {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
};

/**
 * @brief Abstract logger interface.
 *
 * Implementations must be thread-safe — Raft calls log() from multiple threads
 * concurrently (the main Raft loop, RPC handlers, and timer callbacks).
 */
class Logger {
   public:
    virtual ~Logger() = default;

    virtual void log(LogLevel level, std::string_view message) = 0;

    void trace(std::string_view msg) { log(LogLevel::Trace, msg); }
    void debug(std::string_view msg) { log(LogLevel::Debug, msg); }
    void info(std::string_view msg) { log(LogLevel::Info, msg); }
    void warn(std::string_view msg) { log(LogLevel::Warn, msg); }
    void error(std::string_view msg) { log(LogLevel::Error, msg); }
    void fatal(std::string_view msg) { log(LogLevel::Fatal, msg); }
};

/**
 * @brief Creates a logger backed by spdlog that writes to stdout.
 *
 * @param node_id  Included in every log line for multi-node debugging.
 * @param level    Minimum severity to emit.
 */
std::unique_ptr<Logger> create_spdlog_logger(NodeId node_id, LogLevel level = LogLevel::Info);

}  // namespace raft
