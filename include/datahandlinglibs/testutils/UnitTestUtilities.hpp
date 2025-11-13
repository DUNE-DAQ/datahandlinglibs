/**
 * @file UnitTestUtilities.hpp Unit test helper classes
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP

#include "datahandlinglibs/models/DataHandlingModel.hpp"
#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/models/TaskRawDataProcessorModel.hpp"

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
  using typename Base::num_post_processing_delay_max_waits_t;

  void test_run_postprocess_scheduler(
    std::shared_ptr<LatencyBufferType> latency_buffer_impl, std::shared_ptr<RawDataProcessorType> raw_processor_impl,
    std::unique_ptr<folly::Timekeeper> timekeeper, uint64_t post_processing_delay_max_wait)
  {
    this->m_latency_buffer_impl = latency_buffer_impl;
    this->m_raw_processor_impl = raw_processor_impl;
    this->m_timekeeper = std::move(timekeeper);
    this->m_post_processing_delay_max_wait = post_processing_delay_max_wait;
    this->run_postprocess_scheduler();
  }

  num_post_processing_delay_max_waits_t get_num_post_processing_delay_max_waits()
  {
    return this->m_num_post_processing_delay_max_waits.load();
  }

  void set_run_marker(bool run_marker)
  {
    return this->m_run_marker.store(run_marker);
  }
};

} // namespace unittest
} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP
