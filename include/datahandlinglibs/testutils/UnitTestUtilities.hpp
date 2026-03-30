/**
 * @file UnitTestUtilities.hpp Unit test helper classes
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP_

#include "datahandlinglibs/models/DataHandlingModel.hpp"
#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/models/TaskRawDataProcessorModel.hpp"

#include <memory>
#include <utility>

namespace dunedaq {
namespace datahandlinglibs {
namespace unittest {

template<typename ReadoutType,
         typename RequestHandlerType,
         typename LatencyBufferType,
         typename RawDataProcessorType,
         typename InputDataType = ReadoutType>
class MockDataHandlingModel
  : public DataHandlingModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType, InputDataType>
{
public:
  using Base =
    DataHandlingModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType, InputDataType>;
  using Base::Base;
  using Base::PostprocessScheduleAlgorithm;
  using typename Base::num_postprocess_schedule_timeouts_t;
  using typename Base::num_postprocess_late_arrivals_t;
  using typename Base::postprocess_lateness_from_last_processed_t;
  using typename Base::postprocess_lateness_from_newest_t;

  void test_process_item(
    std::shared_ptr<LatencyBufferType> latency_buffer_impl, std::shared_ptr<RawDataProcessorType> raw_processor_impl,
    uint64_t processing_delay_ticks, ReadoutType&& payload) // NOLINT(build/unsigned)
  {
    this->m_latency_buffer_impl = latency_buffer_impl;
    this->m_raw_processor_impl = raw_processor_impl;
    this->m_request_handler_supports_cutoff_timestamp = false;
    this->m_processing_delay_ticks = processing_delay_ticks;
    this->process_item(std::move(payload));
  }

  void test_run_postprocess_scheduler(
    std::shared_ptr<LatencyBufferType> latency_buffer_impl, std::shared_ptr<RawDataProcessorType> raw_processor_impl,
    std::unique_ptr<folly::Timekeeper> timekeeper, uint64_t post_processing_delay_max_wait) // NOLINT(build/unsigned)
  {
    this->m_latency_buffer_impl = latency_buffer_impl;
    this->m_raw_processor_impl = raw_processor_impl;
    this->m_timekeeper = std::move(timekeeper);
    this->m_post_processing_delay_max_wait = post_processing_delay_max_wait;
    this->run_postprocess_scheduler();
  }

  num_postprocess_schedule_timeouts_t get_num_postprocess_schedule_timeouts()
  {
    return this->m_num_postprocess_schedule_timeouts.load();
  }

  num_postprocess_late_arrivals_t get_num_postprocess_late_arrivals()
  {
    return this->m_num_postprocess_late_arrivals.load();
  }  

  postprocess_lateness_from_last_processed_t get_postprocess_lateness_from_last_processed()
  {
    return this->m_postprocess_lateness_from_last_processed.load();
  }    

  postprocess_lateness_from_newest_t get_postprocess_lateness_from_newest()
  {
    return this->m_postprocess_lateness_from_newest.load();
  }    

  void set_run_marker(bool run_marker)
  {
    return this->m_run_marker.store(run_marker);
  }
  
  auto& get_postprocess_state() {
    return this->m_postprocess_state;
  }

};

} // namespace unittest
} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP_
