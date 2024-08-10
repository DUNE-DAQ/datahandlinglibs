/**
 * @file DefaultSkipListRequestHandler.hpp Trigger matching mechanism 
 * used for skip list based LBs in readout models
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_DEFAULTSKIPLISTREQUESTHANDLER_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_DEFAULTSKIPLISTREQUESTHANDLER_HPP_

#include "logging/Logging.hpp"

#include "datahandlinglibs/FrameErrorRegistry.hpp"
#include "datahandlinglibs/DataHandlingIssues.hpp"
#include "datahandlinglibs/ReadoutLogging.hpp"
#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"

#include <atomic>
#include <memory>
#include <string>

using dunedaq::datahandlinglibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace datahandlinglibs {

template<class T>
class DefaultSkipListRequestHandler 
  : public datahandlinglibs::DefaultRequestHandlerModel<T, datahandlinglibs::SkipListLatencyBufferModel<T>>
{
public:
  using inherited = datahandlinglibs::DefaultRequestHandlerModel<T, datahandlinglibs::SkipListLatencyBufferModel<T>>;
  using SkipListAcc = typename folly::ConcurrentSkipList<T>::Accessor;
  using SkipListSkip = typename folly::ConcurrentSkipList<T>::Skipper;

  // Constructor that binds LB and error registry
  DefaultSkipListRequestHandler(std::shared_ptr<datahandlinglibs::SkipListLatencyBufferModel<T>>& latency_buffer,
                                std::shared_ptr<datahandlinglibs::FrameErrorRegistry>& error_registry)
    : datahandlinglibs::DefaultRequestHandlerModel<T, datahandlinglibs::SkipListLatencyBufferModel<T>>(
        latency_buffer,
        error_registry)
  {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "DefaultSkipListRequestHandler created...";
  }

protected:
  // Default ceanup request override
  void cleanup() override { skip_list_cleanup_request(); }

  // Cleanup request implementation for skiplist LB
  void skip_list_cleanup_request(); 

private:
  // Constant: m_max_ts_diff is used to clean old data from the latency buffer. It is set to 10s.
  static const constexpr uint64_t m_max_ts_diff = 625000000; // NOLINT(build/unsigned)

  // Stats
  std::atomic<int> m_found_requested_count{ 0 };
  std::atomic<int> m_bad_requested_count{ 0 };
};

} // namespace datahandlinglibs
} // namespace dunedaq

// Declarations
#include "detail/DefaultSkipListRequestHandler.hxx"

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_DEFAULTSKIPLISTREQUESTHANDLER_HPP_
