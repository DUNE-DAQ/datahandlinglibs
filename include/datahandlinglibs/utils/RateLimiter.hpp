/**
 * @file RateLimiter.hpp Simple RateLimiter implementation
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_UTILS_RATELIMITER_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_UTILS_RATELIMITER_HPP_

#include <atomic>
#include <chrono>
#include <ctime>
#include <unistd.h>

namespace dunedaq {
namespace datahandlinglibs {

/** RateLimiter usage:
 *
 *  auto limiter = RateLimiter(1000); // 1MHz
 *  limiter.init();
 *  while (duration) {
 *    // do work
 *    limiter.limit();
 *  }
 */
/** NOTES:
    This rate limiter is a simple implementation that is intended to be
    used for fast tasks, i.e. tasks that take less or much less time than the
    time intervals (1 / rate) that the RateLimiter is operating with. It doesn't
    work correctly if the tasks take longer than 1 / rate.
 */
class RateLimiter
{
public:
  using timestamp_t = std::uint64_t; // NOLINT(build/unsigned)
  static inline constexpr timestamp_t ns = 1;
  static inline constexpr timestamp_t us = 1000 * ns;
  static inline constexpr timestamp_t ms = 1000 * us;
  static inline constexpr timestamp_t s = 1000 * ms;

  explicit RateLimiter(double kilohertz)
    : m_max_overshoot(10 * ms)
  {
    adjust(kilohertz);
    init();
  }

  void init()
  {
    m_now = gettime();
    m_deadline = m_now + m_period.load();
  }

  /** Optionally: adjust rate from another thread
   *
   *  auto adjuster = std::thread([&]() {
   *    int newrate = 1000;
   *    while (newrate > 0) {
   *      limiter.adjust(newrate);
   *      newrate--;
   *      std::this_thread::sleep_for(std::chrono::seconds(1));
   *    }
   *  }
   */
  void adjust(double kilohertz)
  {
    m_kilohertz.store(kilohertz);
    m_period.store(static_cast<timestamp_t>((1000.f / m_kilohertz) * static_cast<double>(us)));
  }

  void limit()
  {
    m_now = gettime();
    if (m_now > m_deadline + m_max_overshoot) {
      m_deadline = m_now + m_period.load();
    } else {
      if (m_now < m_deadline) {
        if (m_deadline - m_now > 0) {
          timespec tim;
          tim.tv_sec = 0;
          tim.tv_nsec = m_deadline - m_now;
          // The second argument will be overwritten but we
          // do not care about this temporary variable
          nanosleep(&tim, &tim);
          m_now = gettime();
        }
        while (m_now < m_deadline) {
            m_now = gettime();
        }
      }
      m_deadline += m_period.load();
    }
  }

protected:
  timestamp_t gettime()
  {
    ::timespec ts;
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return timestamp_t(ts.tv_sec) * s + timestamp_t(ts.tv_nsec) * ns;
  }

private:
  std::atomic<double> m_kilohertz;
  timestamp_t m_max_overshoot;
  std::atomic<timestamp_t> m_period;
  timestamp_t m_now;
  timestamp_t m_deadline;
};

} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_UTILS_RATELIMITER_HPP_
