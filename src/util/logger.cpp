/**
 * @file logger.cpp
 * @brief spdlog-backed Logger implementation.
 */

#include "raft/util/logger.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <string>

namespace raft {

class SpdlogLogger final : public Logger {
   public:
    SpdlogLogger(NodeId node_id, LogLevel level) {
        auto name = "raft-node-" + std::to_string(node_id.value);
        inner_ = spdlog::stdout_color_mt(name);

        switch (level) {
            case LogLevel::Trace:
                inner_->set_level(spdlog::level::trace);
                break;
            case LogLevel::Debug:
                inner_->set_level(spdlog::level::debug);
                break;
            case LogLevel::Info:
                inner_->set_level(spdlog::level::info);
                break;
            case LogLevel::Warn:
                inner_->set_level(spdlog::level::warn);
                break;
            case LogLevel::Error:
                inner_->set_level(spdlog::level::err);
                break;
            case LogLevel::Fatal:
                inner_->set_level(spdlog::level::critical);
                break;
        }

        inner_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [" + name + "] [%^%l%$] %v");
    }

    void log(LogLevel level, std::string_view message) override {
        switch (level) {
            case LogLevel::Trace:
                inner_->trace("{}", message);
                break;
            case LogLevel::Debug:
                inner_->debug("{}", message);
                break;
            case LogLevel::Info:
                inner_->info("{}", message);
                break;
            case LogLevel::Warn:
                inner_->warn("{}", message);
                break;
            case LogLevel::Error:
                inner_->error("{}", message);
                break;
            case LogLevel::Fatal:
                inner_->critical("{}", message);
                break;
        }
    }

   private:
    std::shared_ptr<spdlog::logger> inner_;
};

std::unique_ptr<Logger> create_spdlog_logger(NodeId node_id, LogLevel level) {
    return std::make_unique<SpdlogLogger>(node_id, level);
}

}  // namespace raft
