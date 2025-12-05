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
    , m_callback_mode(false)
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
    PostprocessScheduleAlgorithm(LatencyBufferType& latency_buffer_impl,
                                 RawDataProcessorType& raw_processor_impl,
                                 uint64_t processing_delay_ticks, // NOLINT(build/unsigned)
                                 uint64_t post_processing_delay_min_wait, // NOLINT(build/unsigned)
                                 uint64_t post_processing_delay_max_wait) // NOLINT(build/unsigned)
      : m_latency_buffer_impl{ latency_buffer_impl }
      , m_raw_processor_impl{ raw_processor_impl }
      , m_processing_delay_ticks{ processing_delay_ticks }
      , m_post_processing_delay_min_wait{ post_processing_delay_min_wait }
      , m_post_processing_delay_max_wait{ post_processing_delay_max_wait }
      , m_first_cycle{ true }
      , m_processed_up_to{}
      , m_last_post_proc_time{ std::chrono::system_clock::now() }
      , m_consecutive_timeouts{ 0 }
      , m_max_wait_in_ticks{ post_processing_delay_max_wait * 62500 } // FIXME: hardcoded clock frequency
    {
    }

    // High-level interface
    // Schedule deferred post-processing and notify timeout expiration to the processor
    int run(bool timeout) {
      int processed = this->do_run(timeout);

      if (timeout) {
        timestamp_t timeout_accumulated = m_consecutive_timeouts * m_max_wait_in_ticks;  
        m_raw_processor_impl.invoke_postprocess_schedule_timeout_policy(timeout_accumulated);
      }

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

      if (m_first_cycle) {
        auto head = m_latency_buffer_impl.front();
        m_processed_up_to.set_timestamp(head->get_timestamp());
        m_first_cycle = false;
        TLOG() << "***** First pass post processing *****";
      }
      
      // Get the LB boundaries
      auto tail = m_latency_buffer_impl.back();
      auto newest_ts = tail->get_timestamp();
          
      timestamp_t end_win_ts = 0;
      std::chrono::time_point<std::chrono::system_clock> now{ std::chrono::system_clock::now() };

      if (timeout) {
        // Return if the last processed timestamp is greater than the newest timestamp
        // This condition occurs after a timeout
        if (m_processed_up_to.get_timestamp() >= newest_ts + 1) {
          TLOG_DEBUG(TLVL_WORK_STEPS) << "Nothing to postprocess (at or past cap)";
          return 0;
        }        

        ++m_consecutive_timeouts;
        timestamp_t timeout_accumulated = m_consecutive_timeouts * m_max_wait_in_ticks;  

        end_win_ts = newest_ts - m_processing_delay_ticks + timeout_accumulated;
        end_win_ts = std::min(end_win_ts, newest_ts + 1); // Cap to prevent end_win_ts from becoming unnecessarily large 
      } else {
        m_consecutive_timeouts = 0;

        if (m_processed_up_to.get_timestamp() >= newest_ts + 1) {
          TLOG_DEBUG(TLVL_WORK_STEPS) << "Nothing to postprocess (data arrived too late, will be ignored)";
          return 0;
        }

        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_post_proc_time);

        if (milliseconds.count() > m_post_processing_delay_min_wait) {
          if (newest_ts - m_processed_up_to.get_timestamp() > m_processing_delay_ticks) {
            end_win_ts = newest_ts - m_processing_delay_ticks;
          } else {
            TLOG_DEBUG(TLVL_WORK_STEPS) << "Not ready to postprocess (m_processing_delay_ticks is greater)";
            return 0;
          }
        } else {
          TLOG_DEBUG(TLVL_WORK_STEPS) << "Not ready to postprocess (too fast)";
          return 0;
        }
      }

      auto start_iter = m_latency_buffer_impl.lower_bound(m_processed_up_to, false);
      m_processed_up_to.set_timestamp(end_win_ts);
      auto end_iter = m_latency_buffer_impl.lower_bound(m_processed_up_to, false);

      // This likely happens when RDT uses a composite key
      // The current algorithm does not support composite keys
      // Our search item `m_processed_up_to` will have its other keys set to their defaults
      // E.g., for TriggerPrimitive, channel = INVALID_TP_CHANNEL
      // Even if an entry with the same ts exists in the buffer, its channel will be a valid value,
      // so `lower_bound` will not be able to find it
      // We should verify that this is the only scenario in which we end up here
      if (!start_iter.good()) { 
        TLOG_DEBUG(TLVL_WORK_STEPS) << "Nothing to postprocess (!start_iter.good())";
        return 0;
      }

      if (start_iter == end_iter) {
        TLOG_DEBUG(TLVL_WORK_STEPS) << "Nothing to postprocess (start_iter == end_iter)";
        return 0;
      }

      int processed = 0;
      for (auto it = start_iter; it != end_iter; ++it) {
        // Just to be completely safe
        // We should understand why we end up here
        if (!it.good()) { 
          TLOG_DEBUG(TLVL_WORK_STEPS) << "Invalid iterator in postprocessing loop";
          return 0;
        }        
        m_raw_processor_impl.postprocess_item(&(*it));
        ++processed;
      }

      m_last_post_proc_time = now;

      return processed;
    }  

  private:
    LatencyBufferType& m_latency_buffer_impl;
    RawDataProcessorType& m_raw_processor_impl;
    const uint64_t m_processing_delay_ticks; // NOLINT(build/unsigned)
    const uint64_t m_post_processing_delay_min_wait; // NOLINT(build/unsigned)
    const uint64_t m_post_processing_delay_max_wait; // NOLINT(build/unsigned)
    bool m_first_cycle;
    RDT m_processed_up_to;
    int m_consecutive_timeouts;
    const timestamp_t m_max_wait_in_ticks;  
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
  bool m_callback_mode;
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
  using num_post_processing_delay_max_waits_t = std::remove_const<std::invoke_result<decltype(&metric_t::num_post_processing_delay_max_waits),metric_t>::type>::type;

  std::atomic<num_payload_t> m_num_payloads{ 0 };
  std::atomic<sum_payload_t> m_sum_payloads{ 0 };
  std::atomic<num_request_t> m_num_requests{ 0 };
  std::atomic<sum_request_t> m_sum_requests{ 0 };
  std::atomic<rawq_timeout_count_t> m_rawq_timeout_count{ 0 };
  std::atomic<num_lb_insert_failures_t> m_num_lb_insert_failures{ 0 };
  std::atomic<num_post_processing_delay_max_waits_t> m_num_post_processing_delay_max_waits{ 0 };
  std::atomic<int> m_stats_packet_count{ 0 };

  // CONSUMER
  utilities::ReusableThread m_consumer_thread;

  // RAW RECEIVER
  std::chrono::milliseconds m_raw_receiver_timeout_ms;
  std::chrono::microseconds m_raw_receiver_sleep_us;
  using raw_receiver_ct = iomanager::ReceiverConcept<InputDataType>;
  std::shared_ptr<raw_receiver_ct> m_raw_data_receiver;
  std::string m_raw_data_receiver_connection_name;

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
