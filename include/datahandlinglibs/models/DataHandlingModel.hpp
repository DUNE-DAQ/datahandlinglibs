/**
 * @file DataHandlingModel.hpp Glue between data source, payload raw processor,
 * latency buffer and request handler.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_READOUTMODEL_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_READOUTMODEL_HPP_

#include "confmodel/DaqModule.hpp"
#include "confmodel/Connection.hpp"
#include "appmodel/DataHandlerModule.hpp"
#include "appmodel/DataMoveCallbackConf.hpp"
#include "appmodel/DataHandlerConf.hpp"
#include "appmodel/RequestHandler.hpp"
#include "appmodel/LatencyBuffer.hpp"
#include "appmodel/DataProcessor.hpp"

#include "datahandlinglibs/opmon/datahandling_info.pb.h"

#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"
#include "iomanager/Receiver.hpp"

#include "logging/Logging.hpp"

#include "daqdataformats/ComponentRequest.hpp"
#include "daqdataformats/Fragment.hpp"

#include "dfmessages/DataRequest.hpp"
#include "dfmessages/TimeSync.hpp"

#include "datahandlinglibs/ReadoutLogging.hpp"
#include "datahandlinglibs/concepts/DataHandlingConcept.hpp"
#include "appmodel/DataHandlerModule.hpp"

#include "datahandlinglibs/DataMoveCallbackRegistry.hpp"
#include "datahandlinglibs/FrameErrorRegistry.hpp"

#include "datahandlinglibs/concepts/LatencyBufferConcept.hpp"
#include "datahandlinglibs/concepts/RawDataProcessorConcept.hpp"
#include "datahandlinglibs/concepts/RequestHandlerConcept.hpp"

#include "datahandlinglibs/DataHandlingIssues.hpp"
#include "utilities/ReusableThread.hpp"

#include <folly/coro/Baton.h>
#include <folly/coro/Task.h>
#include <folly/futures/Future.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using dunedaq::datahandlinglibs::logging::TLVL_QUEUE_POP;
using dunedaq::datahandlinglibs::logging::TLVL_TAKE_NOTE;
using dunedaq::datahandlinglibs::logging::TLVL_TIME_SYNCS;
using dunedaq::datahandlinglibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace datahandlinglibs {

template<class ReadoutType, class RequestHandlerType, class LatencyBufferType, class RawDataProcessorType, class InputDataType = ReadoutType>
class DataHandlingModel : public DataHandlingConcept
{
public:
  // Using shorter typenames
  using RDT = ReadoutType;
  using RHT = RequestHandlerType;
  using LBT = LatencyBufferType;
  using RPT = RawDataProcessorType;
  using IDT = InputDataType;

  // Using timestamp typenames
  using timestamp_t = std::uint64_t; // NOLINT(build/unsigned)
  static inline constexpr timestamp_t ns = 1;
  static inline constexpr timestamp_t us = 1000 * ns;
  static inline constexpr timestamp_t ms = 1000 * us;
  static inline constexpr timestamp_t s = 1000 * ms;

  // Explicit constructor with run marker pass-through
  explicit DataHandlingModel(std::atomic<bool>& run_marker)
    : m_run_marker(run_marker)
    , m_fake_trigger(false)
    , m_current_fake_trigger_id(0)
    , m_consumer_thread(0)
    , m_raw_receiver_timeout_ms(0)
    , m_raw_receiver_sleep_us(0)
    , m_raw_data_receiver(nullptr)
    , m_timesync_thread(0)
    , m_latency_buffer_impl(nullptr)
    , m_raw_processor_impl(nullptr)
  {
  }

  virtual ~DataHandlingModel() = default;

  // Initializes the readoutmodel and its internals
  void init(const appmodel::DataHandlerModule* modconf);

  // Configures the readoutmodel and its internals
  void conf(const appfwk::DAQModule::CommandData_t& args);

  // Unconfigures readoutmodel's internals
  void scrap(const appfwk::DAQModule::CommandData_t& args)
  {
    m_request_handler_impl->scrap(args);
    m_latency_buffer_impl->scrap(args);
    m_raw_processor_impl->scrap(args);
  }

  // Starts readoutmodel's internals
  void start(const appfwk::DAQModule::CommandData_t& args);

  // Stops readoutmodel's internals
  void stop(const appfwk::DAQModule::CommandData_t& args);

  // Record function: invokes request handler's record implementation
  void record(const appfwk::DAQModule::CommandData_t& args) override
  {
    m_request_handler_impl->record(args);
  }

  // Opmon get_info call implementation
  //void get_info(opmonlib::InfoCollector& ci, int level);

  // Consume callback
  std::function<void(IDT&&)> m_consume_callback;

protected:
  class PostprocessScheduleAlgorithm
  {
  public:
    struct PostprocessState {
      timestamp_t next_window_start_ts{ 0 };
      timestamp_t last_processed_ts{ 0 };
    };

    PostprocessScheduleAlgorithm(LatencyBufferType& latency_buffer_impl,
                                 RawDataProcessorType& raw_processor_impl,
                                 uint64_t processing_delay_ticks, // NOLINT(build/unsigned)
                                 uint64_t post_processing_delay_min_wait, // NOLINT(build/unsigned)
                                 uint64_t post_processing_delay_max_wait, // NOLINT(build/unsigned)
                                 PostprocessState& state,
                                 std::mutex& state_mutex) 
      : m_latency_buffer_impl{ latency_buffer_impl }
      , m_raw_processor_impl{ raw_processor_impl }
      , m_processing_delay_ticks{ processing_delay_ticks }
      , m_post_processing_delay_min_wait{ post_processing_delay_min_wait }
      , m_post_processing_delay_max_wait{ post_processing_delay_max_wait }
      , m_first_cycle{ true }
      , m_is_timeout{ false }
      , m_state{ state }
      , m_state_mutex{ state_mutex }
      , m_last_post_proc_time{ std::chrono::system_clock::now() }
      , m_max_wait_in_ticks{ post_processing_delay_max_wait * 62500 } // FIXME: hardcoded clock frequency
    {
    }   

    // High-level interface
    // Schedule deferred post-processing and notify timeout expiration to the processor
    int run(bool timeout) {
      m_is_timeout = timeout;

      int processed = this->do_run();

      // if (timeout) {
      //   timestamp_t timeout_accumulated = m_consecutive_timeouts * m_max_wait_in_ticks;
      //   m_raw_processor_impl.invoke_postprocess_schedule_timeout_policy(timeout_accumulated);
      // }

      return processed;
    }

    // Deferral of the post processing, to allow elements being reordered in the LB
    // Basically, find data older than a certain timestamp and process all data since the last post-processed element up to that value
    int do_run()
    {
      if (m_latency_buffer_impl.occupancy() == 0) {
        TLOG() << "Nothing to postprocess (empty buffer)";
        return 0;
      }

      if (m_first_cycle) { // first data arrival
        auto head = m_latency_buffer_impl.front();
        auto oldest_ts = head->get_timestamp();
        
        set_postprocessing_state(oldest_ts, 0);

        m_first_cycle = false;

        TLOG() << "***** First pass post processing *****";
        return 0;
      }

      // Get the LB boundaries
      auto tail = m_latency_buffer_impl.back();
      const auto newest_ts = tail->get_timestamp();

      const auto next_window_start_ts = get_next_window_start_ts();
      
      if (next_window_start_ts >= newest_ts + 1) {
        TLOG() << "Nothing to postprocess (all items are processed already)";
        return 0;
      }

      auto now = std::chrono::system_clock::now();

      if (!m_is_timeout) { // data arrival
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_post_proc_time);
        if (milliseconds.count() <= m_post_processing_delay_min_wait) {
          TLOG_DEBUG(TLVL_WORK_STEPS) << "Not ready to postprocess (too fast)";
          return 0;
        }        
      }

      RDT next_window_start{};
      next_window_start.set_timestamp(next_window_start_ts);
      auto start_iter = m_latency_buffer_impl.lower_bound(next_window_start, false);

      // This likely happens when RDT uses a composite key
      // The current algorithm does not support composite keys
      // Our search item `next_window_start` will have its other keys set to their defaults
      // E.g., for TriggerPrimitive, channel = INVALID_TP_CHANNEL
      // Even if an entry with the same ts exists in the buffer, its channel will be a valid (smaller) value,
      // so `lower_bound` will not be able to find it
      // We should verify that this is the only scenario in which we end up here
      if (!start_iter.good()) {
        TLOG() << "Nothing to postprocess (!start_iter.good())";
        return 0;
      }

      size_t processed = 0;
      auto last_processed_ts = get_last_processed_ts();
      
      auto window_end_ts = newest_ts + 1; // entire buffer
      auto it = start_iter;

      for (; it.good(); ++it) {
        const auto ts = it->get_timestamp();

        update_timeout_count(ts);

        const auto effective_ts = get_effective_ts(ts);

        if (is_not_ready_for_processing(newest_ts, effective_ts)) {
          window_end_ts = ts;
          break;
        }

        postprocess_item(&(*it), processed, last_processed_ts);
      }

      if (window_end_ts == newest_ts + 1) {
        if (it.good()) {
          TLOG() << "Processing entire buffer";
          postprocess_item(&(*it), processed, last_processed_ts);
        } else {
          TLOG() << "Unexpected delayed postprocessing state: iterator invalid when processing entire buffer";
        }
      } else {
        update_remaining_timeout_counts(it);
      }

      set_postprocessing_state(window_end_ts, last_processed_ts);

      m_last_post_proc_time = now;

      return processed;
    }

  private:
    timestamp_t get_next_window_start_ts()
    {
      std::lock_guard<std::mutex> lock(m_state_mutex);
      return m_state.next_window_start_ts;
    }

    timestamp_t get_last_processed_ts()
    {
      std::lock_guard<std::mutex> lock(m_state_mutex);
      return m_state.last_processed_ts;
    }

    void set_postprocessing_state(timestamp_t next_window_start_ts, timestamp_t last_processed_ts)
    {
      std::lock_guard<std::mutex> lock(m_state_mutex);
      m_state.next_window_start_ts = next_window_start_ts;
      m_state.last_processed_ts = last_processed_ts;
    }

    void postprocess_item(const ReadoutType* item, size_t& processed, timestamp_t& last_processed_ts)
    {
      m_raw_processor_impl.postprocess_item(item);

      const auto ts = item->get_timestamp();
      ++processed;
      last_processed_ts = ts;
      m_timeout_count_map.erase(ts);
    }    

    void update_timeout_count(timestamp_t ts)
    {
      auto& timeout_count = m_timeout_count_map[ts];
      if (m_is_timeout) {
        ++timeout_count;
      }
    }    

    void update_remaining_timeout_counts(auto it)
    {
      if (it.good()) { 
        ++it; // skip 1 because its entry should already be updated in the window finding loop
      } else {
        TLOG() << "Unexpected delayed postprocessing state: iterator invalid before updating remaining timeouts";
      }

      for (; it.good(); ++it) {
        update_timeout_count(it->get_timestamp());
      }
    }    

    int64_t get_effective_ts(timestamp_t ts) const {
      const auto timeout_count = m_timeout_count_map.at(ts);

      const int64_t virtual_age =
        static_cast<int64_t>(timeout_count) * static_cast<int64_t>(m_max_wait_in_ticks);

      return static_cast<int64_t>(ts) - virtual_age;
    }

    bool is_not_ready_for_processing(timestamp_t newest_ts, int64_t effective_ts) const
    {
      const auto newest_ts_signed = static_cast<int64_t>(newest_ts);
      const auto delay_ticks_signed = static_cast<int64_t>(m_processing_delay_ticks);

      return newest_ts_signed - effective_ts <= delay_ticks_signed;
    }    

    LatencyBufferType& m_latency_buffer_impl;
    RawDataProcessorType& m_raw_processor_impl;
    const uint64_t m_processing_delay_ticks; // NOLINT(build/unsigned)
    const uint64_t m_post_processing_delay_min_wait; // NOLINT(build/unsigned)
    const uint64_t m_post_processing_delay_max_wait; // NOLINT(build/unsigned)
    const timestamp_t m_max_wait_in_ticks;
    bool m_first_cycle;
    bool m_is_timeout;
    PostprocessState& m_state;
    std::mutex& m_state_mutex;
    std::map<timestamp_t, std::size_t> m_timeout_count_map;
    std::chrono::time_point<std::chrono::system_clock> m_last_post_proc_time;    
  };

  // Perform processing operations on payload
  void process_item(RDT&& payload);

  // Transform payload if needed, then perform processing
  void transform_and_process(IDT&& payload);

  // Raw data consume callback
  void consume_callback(IDT&& payload);

  // Raw data consumer's work function
  void run_consume();

  // Timesync thread's work function
  void run_timesync();

  // Postprocess scheduler thread's work function
  void run_postprocess_scheduler();

  // Postprocess schedule coroutine
  folly::coro::Task<void> postprocess_schedule();

  // Dispatch data request
  void dispatch_requests(dfmessages::DataRequest& data_request);

  // Transform input data type to readout
  virtual std::vector<RDT> transform_payload(IDT& original) const
  {
    return { reinterpret_cast<RDT&>(original) };
  }

  // Actions postprocess scheduler takes if no data arrives in a configured time
  virtual void invoke_postprocess_schedule_timeout_policy() const
  {
    return; // No-op for this class
  }

  // Operational monitoring
  virtual void generate_opmon_data() override;

  // Constructor params
  std::atomic<bool>& m_run_marker;

  // CONFIGURATION
  //appfwk::app::ModInit m_queue_config;
  bool m_fake_trigger;
  bool m_generate_timesync = false;
  int m_current_fake_trigger_id;
  daqdataformats::SourceID m_sourceid;
  daqdataformats::run_number_t m_run_number;
  uint64_t m_processing_delay_ticks; // NOLINT(build/unsigned)
  uint64_t m_post_processing_delay_min_wait; // NOLINT(build/unsigned)
  uint64_t m_post_processing_delay_max_wait; // NOLINT(build/unsigned)

  // STATS
  using metric_t = dunedaq::datahandlinglibs::opmon::DataHandlerInfo;
  using num_payload_t = std::remove_const<std::invoke_result<decltype(&metric_t::num_payloads),metric_t>::type>::type;
  using sum_payload_t = std::remove_const<std::invoke_result<decltype(&metric_t::sum_payloads),metric_t>::type>::type;
  using num_request_t = std::remove_const<std::invoke_result<decltype(&metric_t::num_requests),metric_t>::type>::type;
  using sum_request_t = std::remove_const<std::invoke_result<decltype(&metric_t::sum_requests),metric_t>::type>::type;
  using rawq_timeout_count_t = std::remove_const<std::invoke_result<decltype(&metric_t::num_data_input_timeouts),metric_t>::type>::type;
  using num_lb_insert_failures_t = std::remove_const<std::invoke_result<decltype(&metric_t::num_lb_insert_failures),metric_t>::type>::type;
  using num_postprocess_schedule_timeouts_t = std::remove_const<std::invoke_result<decltype(&metric_t::num_postprocess_schedule_timeouts),metric_t>::type>::type;
  using num_postprocess_late_arrivals_t = std::remove_const<std::invoke_result<decltype(&metric_t::num_postprocess_late_arrivals),metric_t>::type>::type;
  using postprocess_lateness_from_last_processed_t = std::remove_const<std::invoke_result<decltype(&metric_t::postprocess_lateness_from_last_processed),metric_t>::type>::type;
  using postprocess_lateness_from_newest_t = std::remove_const<std::invoke_result<decltype(&metric_t::postprocess_lateness_from_newest),metric_t>::type>::type;

  std::atomic<num_payload_t> m_num_payloads{ 0 };
  std::atomic<sum_payload_t> m_sum_payloads{ 0 };
  std::atomic<num_request_t> m_num_requests{ 0 };
  std::atomic<sum_request_t> m_sum_requests{ 0 };
  std::atomic<rawq_timeout_count_t> m_rawq_timeout_count{ 0 };
  std::atomic<num_lb_insert_failures_t> m_num_lb_insert_failures{ 0 };
  std::atomic<num_postprocess_schedule_timeouts_t> m_num_postprocess_schedule_timeouts{ 0 };
  std::atomic<num_postprocess_late_arrivals_t> m_num_postprocess_late_arrivals{ 0 };
  std::atomic<postprocess_lateness_from_last_processed_t> m_postprocess_lateness_from_last_processed{ 0 };
  std::atomic<postprocess_lateness_from_newest_t> m_postprocess_lateness_from_newest{ 0 };
  std::atomic<int> m_stats_packet_count{ 0 };

  // CONSUMER
  utilities::ReusableThread m_consumer_thread;

  // RAW RECEIVER
  std::chrono::milliseconds m_raw_receiver_timeout_ms;
  std::chrono::microseconds m_raw_receiver_sleep_us;
  using raw_receiver_ct = iomanager::ReceiverConcept<InputDataType>;
  std::shared_ptr<raw_receiver_ct> m_raw_data_receiver;
  std::string m_raw_data_receiver_connection_name;
  const appmodel::DataMoveCallbackConf* m_raw_data_callback_conf;

  // REQUEST RECEIVERS
  using request_receiver_ct = iomanager::ReceiverConcept<dfmessages::DataRequest>;
  std::shared_ptr<request_receiver_ct> m_data_request_receiver;

  // FRAGMENT SENDER
  //std::chrono::milliseconds m_fragment_sender_timeout_ms;
  //using fragment_sender_ct = iomanager::SenderConcept<std::pair<std::unique_ptr<daqdataformats::Fragment>, std::string>>;
  //std::shared_ptr<fragment_sender_ct> m_fragment_sender;

  // TIME-SYNC
  using timesync_sender_ct = iomanager::SenderConcept<dfmessages::TimeSync>; // no timeout -> published
  std::shared_ptr<timesync_sender_ct> m_timesync_sender;
  utilities::ReusableThread m_timesync_thread;
  std::string m_timesync_connection_name;

  // POSTPROCESS SCHEDULER
  utilities::ReusableThread m_postprocess_scheduler_thread;
  folly::coro::Baton m_baton;
  std::unique_ptr<folly::Timekeeper> m_timekeeper;
  PostprocessScheduleAlgorithm::PostprocessState m_postprocess_state;
  std::mutex m_postprocess_state_mutex;  

  // LATENCY BUFFER
  std::shared_ptr<LatencyBufferType> m_latency_buffer_impl;

  // RAW PROCESSING
  std::shared_ptr<RawDataProcessorType> m_raw_processor_impl;

  // REQUEST HANDLER
  std::shared_ptr<RequestHandlerType> m_request_handler_impl;
  bool m_request_handler_supports_cutoff_timestamp;

  // ERROR REGISTRY
  std::unique_ptr<FrameErrorRegistry> m_error_registry;

  // RUN START T0
  std::chrono::time_point<std::chrono::high_resolution_clock> m_t0;
};

} // namespace datahandlinglibs
} // namespace dunedaq

// Declarations
#include "detail/DataHandlingModel.hxx"

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_READOUTMODEL_HPP_
