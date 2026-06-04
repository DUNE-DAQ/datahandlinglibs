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
#include "datahandlinglibs/models/DefaultSkipListRequestHandler.hpp"
#include "datahandlinglibs/models/TaskRawDataProcessorModel.hpp"

#include <memory>
#include <utility>

namespace dunedaq {
namespace datahandlinglibs {
namespace unittest {

template<class ReadoutType, class RequestHandlerType>
class MockRequestHandlerType : public RequestHandlerType
{
public:
  using RequestHandlerType::RequestHandlerType;

  void test_periodic_cleanups() { this->periodic_cleanups(); }

  void test_simulate_issuing()
  {
    m_issuing_calls++;

    {
      std::unique_lock<std::mutex> lock(this->m_cv_mutex);
      this->m_cv.wait(lock, [&] { return !this->m_cleanup_requested; });
      this->m_requests_running++;
    }
    this->m_cv.notify_all();

    auto back = this->m_latency_buffer->back();
    if (back != nullptr) {
      ReadoutType element_to_search;
      element_to_search.set_timestamp(back->get_timestamp());

      auto chunk_iter = this->m_latency_buffer->lower_bound(element_to_search);
      auto end = this->m_latency_buffer->end();

      size_t seen = 0;
      for (; chunk_iter != end && chunk_iter.good() && seen < 100; ++chunk_iter) {
        ++seen;
      }
    }

    {
      std::lock_guard<std::mutex> lock(this->m_cv_mutex);
      this->m_requests_running--;
    }
    this->m_cv.notify_all();
  }

  void test_simulate_recording()
  {
    auto front = this->m_latency_buffer->front();
    if (front == nullptr) {
      return;
    }

    m_recording_calls++;

    ReadoutType element_to_search;
    element_to_search.set_timestamp(front->get_timestamp());

    {
      std::unique_lock<std::mutex> lock(this->m_cv_mutex);
      this->m_cv.wait(lock, [&] { return !this->m_cleanup_requested; });
      this->m_requests_running++;
    }
    this->m_cv.notify_all();

    auto chunk_iter = this->m_latency_buffer->lower_bound(element_to_search, true);
    auto end = this->m_latency_buffer->end();

    {
      std::lock_guard<std::mutex> lock(this->m_cv_mutex);
      this->m_requests_running--;
    }
    this->m_cv.notify_all();

    size_t seen = 0;
    for (; chunk_iter != end && chunk_iter.good() && seen < 100; ++chunk_iter) {
      ++seen;
    }
  }

  uint64_t get_num_buffer_cleanups() const // NOLINT(build/unsigned)
  {
    return this->m_num_buffer_cleanups.load();
  }

  uint64_t get_pops_count() const // NOLINT(build/unsigned)
  {
    return this->m_pops_count.load();
  }

  uint64_t get_issuing_calls() const { return m_issuing_calls.load(); } // NOLINT(build/unsigned)

  uint64_t get_recording_calls() const { return m_recording_calls.load(); } // NOLINT(build/unsigned)

  void set_run_marker(bool run_marker) { this->m_run_marker.store(run_marker); }

  void set_pop_limit_size(unsigned pop_limit_size) { this->m_pop_limit_size = pop_limit_size; }

  void set_pop_size_pct(float pop_size_pct) { this->m_pop_size_pct = pop_size_pct; }

private:
  std::atomic<uint64_t> m_recording_calls{ 0 }; // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_issuing_calls{ 0 };   // NOLINT(build/unsigned)
};

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

  void test_run_postprocess_scheduler(std::shared_ptr<LatencyBufferType> latency_buffer_impl,
                                      std::shared_ptr<RawDataProcessorType> raw_processor_impl,
                                      std::unique_ptr<folly::Timekeeper> timekeeper,
                                      uint64_t post_processing_delay_max_wait)
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

  void set_run_marker(bool run_marker) { return this->m_run_marker.store(run_marker); }
};

} // namespace unittest
} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP
