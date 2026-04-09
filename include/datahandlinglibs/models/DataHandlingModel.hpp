/**
 * @file DataHandlingModel.hpp Glue between data source, payload raw processor,
 * latency buffer and request handler.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_DATAHANDLINGMODEL_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_DATAHANDLINGMODEL_HPP_

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
#include <map>

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
      // Where to start processing in the next iteration
      std::atomic<timestamp_t> next_window_start_ts{ 0 };

      std::atomic<timestamp_t> last_processed_ts{ 0 };
    };

    PostprocessScheduleAlgorithm(LatencyBufferType& latency_buffer_impl,
                                 RawDataProcessorType& raw_processor_impl,
                                 uint64_t processing_delay_ticks, // NOLINT(build/unsigned)
                                 uint64_t post_processing_delay_min_wait_ms, // NOLINT(build/unsigned)
                                 uint64_t post_processing_delay_max_wait_ms, // NOLINT(build/unsigned)
                                 PostprocessState& state) 
      : m_latency_buffer_impl{ latency_buffer_impl }
      , m_raw_processor_impl{ raw_processor_impl }
      , m_processing_delay_ticks{ processing_delay_ticks }
      , m_post_processing_delay_min_wait_ms{ post_processing_delay_min_wait_ms }
      , m_post_processing_delay_max_wait_ms{ post_processing_delay_max_wait_ms }
      , m_max_wait_in_ticks{ post_processing_delay_max_wait_ms * 62500 } // FIXME: hardcoded clock frequency
      , m_first_cycle{ true }
      , m_state{ state }
      , m_last_post_proc_time{ std::chrono::system_clock::now() }
    {
    }   

    // High-level interface
    // Schedule deferred post-processing and notify timeout expiration to the processor
    int run(bool timeout) {
      int processed = this->do_run(timeout);

      // if (timeout) {
      //   timestamp_t timeout_accumulated = m_consecutive_timeouts * m_max_wait_in_ticks;
      //   m_raw_processor_impl.invoke_postprocess_schedule_timeout_policy(timeout_accumulated);
      // }

      return processed;
    }

    // Deferral of the post processing, to allow elements being reordered in the LB
    // Basically, find data older than a certain timestamp and process all data since the last post-processed element up to that value
    int do_run(bool timeout)
    {
      if (m_latency_buffer_impl.occupancy() == 0) {
        TLOG_DEBUG(TLVL_WORK_STEPS) << "Nothing to postprocess (empty buffer)";
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

      auto tail = m_latency_buffer_impl.back();
      const auto newest_ts = tail->get_timestamp();

      const auto next_window_start_ts = m_state.next_window_start_ts.load(std::memory_order_relaxed);
      
      // This happens if the entire buffer was already processed in a previous iteration
      // and no newer data arrived since then.
      // e.g. Buffer: {1, 3}. We already processed {1, 3}. Now {2} arrives.
      //      We notice this because next window start is {4} (last processed + 1) and 4 > 3.   
      if (next_window_start_ts > newest_ts) {
        TLOG_DEBUG(TLVL_WORK_STEPS) << "Postprocessing window is already closed";
        return 0;
      }

      auto now = std::chrono::system_clock::now();

      if (!timeout) { // data arrival
        if (now - m_last_post_proc_time <= std::chrono::milliseconds(m_post_processing_delay_min_wait_ms)) {
          TLOG_DEBUG(TLVL_WORK_STEPS) << "Not enough time passed since last postprocessing";
          return 0;
        }        
      }

      RDT next_window_start{}; // braces are essential for value initialization
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
        TLOG_DEBUG(TLVL_WORK_STEPS) << "Postprocessing next window start cannot be found in the buffer (known issue with composite keys)";
        return 0;
      }

      size_t processed = 0;
      
      timestamp_t window_end_ts = 0;
      bool processed_entire_buffer = true;
      auto last_processed_ts = m_state.last_processed_ts.load(std::memory_order_relaxed);
      auto it = start_iter;

      for (; it.good(); ++it) {
        const auto current_ts = it->get_timestamp();

        if (timeout) {
          increment_timeout_count(current_ts);
        }

        if (is_too_early_to_postprocess(current_ts, newest_ts)) {
          window_end_ts = current_ts;
          processed_entire_buffer = false;
          break;
        }

        postprocess_item(&(*it), processed, last_processed_ts);
      }
      
      if (timeout) {
        if (processed_entire_buffer) {
          TLOG_DEBUG(TLVL_WORK_STEPS) << "Entire buffer is postprocessed";
          window_end_ts = last_processed_ts + 1; // The loop didn't break and `window_end_ts` wasn't set.
        } else {
          increment_remaining_timeout_counts(it);
        }
      }
      
      // Make `window_end_ts` the next window start.
      set_postprocessing_state(window_end_ts, last_processed_ts);

      m_last_post_proc_time = now;

      return processed;
    }

  private:

    void set_postprocessing_state(timestamp_t next_window_start_ts, timestamp_t last_processed_ts)
    {
      m_state.next_window_start_ts.store(next_window_start_ts, std::memory_order_relaxed);
      m_state.last_processed_ts.store(last_processed_ts, std::memory_order_relaxed);
    }

    void postprocess_item(const ReadoutType* item, size_t& processed, timestamp_t& last_processed_ts)
    {
      m_raw_processor_impl.postprocess_item(item);

      const auto ts = item->get_timestamp();
      ++processed;
      last_processed_ts = ts;
      m_timeout_count_map.erase(ts);
    }    

    void increment_timeout_count(timestamp_t ts)
    {
      ++m_timeout_count_map[ts];
    }    

    void increment_remaining_timeout_counts(auto it)
    {
      if (it.good()) {
        ++it; // skip 1 because its entry should already be updated in the end window finding loop
      } else [[unlikely]] {
        TLOG_DEBUG(TLVL_WORK_STEPS) << "Unexpected delayed postprocessing state: iterator invalid before updating remaining timeouts";
      }

      for (; it.good(); ++it) {
        increment_timeout_count(it->get_timestamp());
      }
    }

    size_t get_timeout_count(timestamp_t ts) const
    {
      auto it = m_timeout_count_map.find(ts);
      return (it != m_timeout_count_map.end()) ? it->second : 0;
    }    

    bool is_too_early_to_postprocess(timestamp_t ts, timestamp_t newest_ts) const
    {
      // An item can be processed if it is "old enough" relative to the newest timestamp. 
      // The age difference must be bigger than the delay ticks we configure; otherwise, it is too early to process.
      const auto age_diff = newest_ts - ts;

      // Timeouts artificially make items older; that is why, on top of the (real) age difference,
      // we add the virtual age of the item.
      const auto virtual_age = get_timeout_count(ts) * m_max_wait_in_ticks;

      return age_diff + virtual_age <= m_processing_delay_ticks;
    }  

    LatencyBufferType& m_latency_buffer_impl;
    RawDataProcessorType& m_raw_processor_impl;
    const uint64_t m_processing_delay_ticks; // NOLINT(build/unsigned)
    const uint64_t m_post_processing_delay_min_wait_ms; // NOLINT(build/unsigned)
    const uint64_t m_post_processing_delay_max_wait_ms; // NOLINT(build/unsigned)
    const timestamp_t m_max_wait_in_ticks;
    bool m_first_cycle;
    PostprocessState& m_state;
    // Keeps track of timeout counts of not-yet-processed items.
    // This is required because not every item waits in the buffer the same amount of time.
    // This number will be used to decide if an item can be processed.
    std::map<timestamp_t, std::size_t> m_timeout_count_map;
    std::chrono::time_point<std::chrono::system_clock> m_last_post_proc_time;    
  };

  // Perform processing operations on payload
  void process_item(RDT&& payload);

  // Update monitoring variables of delayed postprocessing
  void update_postprocess_monitoring(timestamp_t payload_ts);

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
  void generate_opmon_data() override;

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
  uint64_t m_post_processing_delay_min_wait_ms; // NOLINT(build/unsigned)
  uint64_t m_post_processing_delay_max_wait_ms; // NOLINT(build/unsigned)
  bool m_post_processing_delay_monitor_late_tick_diffs_only;
  
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
  using max_postprocess_tick_diff_to_next_window_start_t = std::remove_const<std::invoke_result<decltype(&metric_t::max_postprocess_tick_diff_to_next_window_start),metric_t>::type>::type;
  using max_postprocess_tick_diff_to_newest_t = std::remove_const<std::invoke_result<decltype(&metric_t::max_postprocess_tick_diff_to_newest),metric_t>::type>::type;
  using max_postprocess_tick_diff_to_last_processed_t = std::remove_const<std::invoke_result<decltype(&metric_t::max_postprocess_tick_diff_to_last_processed),metric_t>::type>::type;

  std::atomic<num_payload_t> m_num_payloads{ 0 };
  std::atomic<sum_payload_t> m_sum_payloads{ 0 };
  std::atomic<num_request_t> m_num_requests{ 0 };
  std::atomic<sum_request_t> m_sum_requests{ 0 };
  std::atomic<rawq_timeout_count_t> m_rawq_timeout_count{ 0 };
  std::atomic<num_lb_insert_failures_t> m_num_lb_insert_failures{ 0 };
  std::atomic<num_postprocess_schedule_timeouts_t> m_num_postprocess_schedule_timeouts{ 0 };
  std::atomic<num_postprocess_late_arrivals_t> m_num_postprocess_late_arrivals{ 0 };
  std::atomic<max_postprocess_tick_diff_to_next_window_start_t> m_max_postprocess_tick_diff_to_next_window_start{ 0 };
  std::atomic<max_postprocess_tick_diff_to_newest_t> m_max_postprocess_tick_diff_to_newest{ 0 };
  std::atomic<max_postprocess_tick_diff_to_last_processed_t> m_max_postprocess_tick_diff_to_last_processed{ 0 };
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

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_DATAHANDLINGMODEL_HPP_
