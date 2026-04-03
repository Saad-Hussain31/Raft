/**
 * @file timer.cpp
 * @brief Wall-clock timer implementation using std::jthread + condition_variable.
 */

#include "raft/util/timer.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace raft {

class RealTimer final : public Timer {
   public:
    ~RealTimer() override { stop(); }

    void reset(Duration duration, Callback callback) override {
        // TODO: Implement timer reset.
        //
        //   1. Lock mutex_.
        //   2. Store the callback and compute the deadline:
        //        deadline_ = std::chrono::steady_clock::now() + duration.
        //   3. Set active_ = true.
        //   4. If the timer thread is not running, launch it.
        //   5. Notify the condition variable to wake the thread
        //      (it may be waiting on the old deadline).
        //
        // The timer thread waits on cv_ until the deadline.
        //   - If the deadline passes, invoke the callback and set active_ = false.
        //   - If the cv is notified (reset or stop), re-check.
        //
        // Thread safety: reset() may be called from any thread.
        (void)duration;
        (void)callback;
    }

    void stop() override {
        // TODO: Cancel the timer.
        //
        //   1. Lock mutex_.
        //   2. Set active_ = false, shutdown_ = true.
        //   3. Notify cv_.
        //   4. Unlock and join the thread (if running).
        //
        // Postcondition: the callback will NOT fire after stop() returns.
    }

    bool running() const override {
        // TODO: Return active_ under mutex_ protection.
        return false;
    }

   private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool active_{false};
    bool shutdown_{false};
    Callback callback_;
    std::chrono::steady_clock::time_point deadline_;
    std::jthread thread_;
};

class RealTimerFactory final : public TimerFactory {
   public:
    std::unique_ptr<Timer> create() override { return std::make_unique<RealTimer>(); }
};

std::unique_ptr<TimerFactory> create_real_timer_factory() {
    return std::make_unique<RealTimerFactory>();
}

}  // namespace raft
