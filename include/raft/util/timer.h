/**
 * @file timer.h
 * @brief Election and heartbeat timer management.
 *
 * The Raft protocol relies on two timeout mechanisms:
 *
 *   1. **Election timeout** — Each follower / candidate starts a timer with a
 *      random duration in [min, max].  If no AppendEntries or vote grant is
 *      received before the timer fires, the node starts a new election.
 *      Randomisation prevents split votes.
 *
 *   2. **Heartbeat interval** — The leader sends empty AppendEntries RPCs at
 *      this fixed interval to prevent followers from timing out.
 *      Invariant:  heartbeat_interval << election_timeout_min.
 *
 * Design:
 *   - The timer runs on a background thread.
 *   - It invokes a user-supplied callback when the deadline expires.
 *   - reset() restarts the timer with a new random duration (for election
 *     timeout) or the fixed heartbeat interval.
 *   - stop() cancels the timer; after stop() the callback will never fire.
 *   - The implementation must be thread-safe.
 */

#pragma once

#include <chrono>
#include <functional>
#include <memory>

namespace raft {

/**
 * @brief Abstract timer interface.
 *
 * Concrete implementations can use std::jthread + condition_variable, or an
 * event loop, or an io_context, as long as the contract is met.
 */
class Timer {
   public:
    using Callback = std::function<void()>;
    using Duration = std::chrono::milliseconds;

    virtual ~Timer() = default;

    /**
     * @brief Start or restart the timer.
     *
     * If the timer is already running, the previous deadline is cancelled and
     * replaced.  The callback will fire once after @p duration unless reset()
     * or stop() is called first.
     *
     * @param duration  Time until the callback fires.
     * @param callback  Invoked exactly once when the timer expires.
     *
     * Thread safety: may be called from any thread.
     */
    virtual void reset(Duration duration, Callback callback) = 0;

    /**
     * @brief Cancel the timer.  The callback will NOT fire after this returns.
     *
     * Thread safety: may be called from any thread.  Must be safe to call even
     * if the timer is not running.
     */
    virtual void stop() = 0;

    /** @brief Returns true if the timer is currently ticking. */
    virtual bool running() const = 0;
};

/**
 * @brief Configuration for election and heartbeat timers.
 *
 * The election timeout is randomly chosen from [election_timeout_min,
 * election_timeout_max] each time it is reset.  Per the paper, a typical
 * range is 150–300 ms.  The heartbeat interval must be significantly less
 * than the minimum election timeout.
 */
struct TimerConfig {
    Timer::Duration election_timeout_min{150};
    Timer::Duration election_timeout_max{300};
    Timer::Duration heartbeat_interval{50};
};

/**
 * @brief Factory for creating Timer instances.
 *
 * Separating creation from usage allows tests to substitute deterministic
 * timers.
 */
class TimerFactory {
   public:
    virtual ~TimerFactory() = default;

    /** @brief Create a new timer instance. */
    virtual std::unique_ptr<Timer> create() = 0;
};

/**
 * @brief Creates a TimerFactory backed by real wall-clock timers.
 */
std::unique_ptr<TimerFactory> create_real_timer_factory();

}  // namespace raft
