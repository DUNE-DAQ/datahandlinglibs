/**
 * @file RawWIBTp.hpp Error registry for missing frames
 *
 * This is part of the DUNE DAQ , copyright 2021.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_FRAMEERRORREGISTRY_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_FRAMEERRORREGISTRY_HPP_

#include <cstdint> // uint_t types
#include <map>
#include <mutex>
#include <utility>
#include <string>

namespace dunedaq {
namespace datahandlinglibs {

class FrameErrorRegistry
{
public:
  struct ErrorInterval
  {
  public:
    ErrorInterval(uint64_t start_ts, uint64_t end_ts) // NOLINT(build/unsigned)
      : start_ts(start_ts)
      , end_ts(end_ts)
    {}

    uint64_t start_ts; // NOLINT(build/unsigned)
    uint64_t end_ts;   // NOLINT(build/unsigned)

    bool operator<(const ErrorInterval& other) const { return end_ts < other.end_ts; }

    bool operator>(const ErrorInterval& other) const { return end_ts > other.end_ts; }
  };

  FrameErrorRegistry()
    : m_errors()
  {}

  void add_error(std::string error_name, ErrorInterval error)
  {
    std::lock_guard<std::mutex> guard(m_error_map_mutex);
    if (m_errors.find(error_name) == m_errors.end()) {
      TLOG("FrameErrorRegistry") << "Encountered new error, name=\"" << error_name << "\"";
    }
    m_errors.erase(error_name);
    m_errors.insert(std::make_pair(error_name, error));
  }

  void remove_errors_until(uint64_t ts) // NOLINT(build/unsigned)
  {
    std::lock_guard<std::mutex> guard(m_error_map_mutex);
    for (auto it = m_errors.begin(); it != m_errors.end();) {
      if (ts > it->second.end_ts) {
        std::string error_name = it->first;
        it = m_errors.erase(it);
        TLOG("FrameErrorRegistry") << "Removed error, name=\"" << error_name << "\"";
      } else {
        it++;
      }
    }
  }

  bool has_error(std::string error_name) { return m_errors.find(error_name) != m_errors.end(); }

  bool has_error() { return !m_errors.empty(); }

private:
  std::map<std::string, ErrorInterval> m_errors;
  std::mutex m_error_map_mutex;
};

} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_FRAMEERRORREGISTRY_HPP_
