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
#include "datahandlinglibs/utils/ReusableThread.hpp"

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
    m_pid_of_current_process = getpid();
  }

  // Initializes the readoutmodel and its internals
  void init(const appmodel::DataHandlerModule* modconf);

  // Configures the readoutmodel and its internals
  void conf(const nlohmann::json& args);

  // Unconfigures readoutmodel's internals
  void scrap(const nlohmann::json& args)
  {
    m_request_handler_impl->scrap(args);
    m_latency_buffer_impl->scrap(args);
    m_raw_processor_impl->scrap(args);
  }

  // Starts readoutmodel's internals
  void start(const nlohmann::json& args);

  // Stops readoutmodel's internals
  void stop(const nlohmann::json& args);

  // Record function: invokes request handler's record implementation
  void record(const nlohmann::json& args) override 
  { 
    m_request_handler_impl->record(args); 
  }

  // Opmon get_info call implementation
  //void get_info(opmonlib::InfoCollector& ci, int level);

  // Raw data consume callback
  void consume_payload(RDT&& payload);

  // Consume callback
  std::function<void(RDT&&)> m_consume_callback;

protected:
 
  // Raw data consumer's work function
  void run_consume();

  // Timesync thread's work function
  void run_timesync();

  // Dispatch data request
  void dispatch_requests(dfmessages::DataRequest& data_request);

  // Transform input data type to readout
  RDT& transform_payload(IDT& payload) const
  {
    return reinterpret_cast<RDT&>(payload);
  }

  // Operational monitoring
  virtual void generate_opmon_data() override;

  // Constuctor params
  std::atomic<bool>& m_run_marker;

  // CONFIGURATION
  //appfwk::app::ModInit m_queue_config;
  bool m_callback_mode;
  bool m_fake_trigger;
  bool m_generate_timesync = false;
  int m_current_fake_trigger_id;
  daqdataformats::SourceID m_sourceid;
  daqdataformats::run_number_t m_run_number;
  uint64_t m_processing_delay_ticks;
  // STATS
  std::atomic<int> m_num_payloads{ 0 };
  std::atomic<int> m_sum_payloads{ 0 };
  std::atomic<int> m_num_requests{ 0 };
  std::atomic<int> m_sum_requests{ 0 };
  std::atomic<int> m_rawq_timeout_count{ 0 };
  std::atomic<int> m_stats_packet_count{ 0 };
  std::atomic<int> m_num_payloads_overwritten{ 0 };

  // CONSUMER
  ReusableThread m_consumer_thread;

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
  ReusableThread m_timesync_thread;
  std::string m_timesync_connection_name;
  uint32_t m_pid_of_current_process;

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
