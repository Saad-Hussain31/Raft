/**
 * @file metrics.cpp
 * @brief In-memory metrics store using atomic counters.
 */

#include "raft/util/metrics.h"

#include <mutex>
#include <string>
#include <unordered_map>

namespace raft {

class InMemoryMetrics final : public Metrics {
   public:
    void increment_counter(std::string_view name, uint64_t delta) override {
        std::lock_guard lock(mutex_);
        counters_[std::string(name)] += delta;
    }

    void set_gauge(std::string_view name, int64_t value) override {
        std::lock_guard lock(mutex_);
        gauges_[std::string(name)] = value;
    }

    int64_t get_gauge(std::string_view name) const override {
        std::lock_guard lock(mutex_);
        auto it = gauges_.find(std::string(name));
        return it != gauges_.end() ? it->second : 0;
    }

    uint64_t get_counter(std::string_view name) const override {
        std::lock_guard lock(mutex_);
        auto it = counters_.find(std::string(name));
        return it != counters_.end() ? it->second : 0;
    }

   private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, uint64_t> counters_;
    std::unordered_map<std::string, int64_t> gauges_;
};

std::unique_ptr<Metrics> create_in_memory_metrics() {
    return std::make_unique<InMemoryMetrics>();
}

}  // namespace raft
