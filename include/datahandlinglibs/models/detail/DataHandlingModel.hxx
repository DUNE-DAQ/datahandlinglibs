// Declarations for DataHandlingModel

#include <typeinfo>

namespace dunedaq {
namespace datahandlinglibs {

template<class RDT, class RHT, class LBT, class RPT>
void 
DataHandlingModel<RDT, RHT, LBT, RPT>::init(const appmodel::DataHandlerModule* mcfg)
{
  // Setup request queues
  //setup_request_queues(mcfg);
  try {
    for (auto input : mcfg->get_inputs()) {
      if (input->get_data_type() == "DataRequest") {
        m_data_request_receiver = get_iom_receiver<dfmessages::DataRequest>(input->UID()) ;
      }
      else {
	m_raw_data_receiver_connection_name = input->UID();
        // Parse for prefix
        std::string conn_name = input->UID(); 
        const char delim = '_';
        std::vector<std::string> words;
        std::size_t start;
        std::size_t end = 0;
        while ((start = conn_name.find_first_not_of(delim, end)) != std::string::npos) {
          end = conn_name.find(delim, start);
          words.push_back(conn_name.substr(start, end - start));
        }

	TLOG_DEBUG(TLVL_WORK_STEPS) << "Initialize connection based on uid: " 
          << m_raw_data_receiver_connection_name << " front word: " << words.front();

	std::string cb_prefix("cb");
        if (words.front() == cb_prefix) {
          m_callback_mode = true;
        }

        if (!m_callback_mode) {
	  m_raw_data_receiver = get_iom_receiver<RDT>(m_raw_data_receiver_connection_name);
          m_raw_receiver_timeout_ms = std::chrono::milliseconds(input->get_recv_timeout_ms());
        }
      }
    }
    for (auto output : mcfg->get_outputs()) {
      if (output->get_data_type() == "TimeSync") {
        m_generate_timesync = true;
        m_timesync_sender = get_iom_sender<dfmessages::TimeSync>(output->UID()) ;
        m_timesync_connection_name = output->UID();
        break;
      }
    }
  } catch (const ers::Issue& excpt) {
    throw ResourceQueueError(ERS_HERE, "raw_input or frag_output", "DataHandlingModel", excpt);
  }

  // Raw input connection sensibility check
  if (!m_callback_mode && m_raw_data_receiver == nullptr) {
    ers::error(ConfigurationError(ERS_HERE, m_sourceid, "Non callback mode, and receiver is unset!"));
  }

  // Instantiate functionalities
  m_error_registry.reset(new FrameErrorRegistry());
  m_latency_buffer_impl.reset(new LBT());
  m_raw_processor_impl.reset(new RPT(m_error_registry, mcfg->get_post_processing_enabled()));
  m_request_handler_impl.reset(new RHT(m_latency_buffer_impl, m_error_registry));

  register_node(mcfg->get_module_configuration()->get_latency_buffer()->UID(), m_latency_buffer_impl);
  register_node(mcfg->get_module_configuration()->get_data_processor()->UID(), m_raw_processor_impl);
  register_node(mcfg->get_module_configuration()->get_request_handler()->UID(), m_request_handler_impl);

  //m_request_handler_impl->init(args);
  //m_raw_processor_impl->init(args);
  m_request_handler_supports_cutoff_timestamp = m_request_handler_impl->supports_cutoff_timestamp();
  m_fake_trigger = false;
  m_raw_receiver_sleep_us = std::chrono::microseconds::zero();
  m_sourceid.id = mcfg->get_source_id();
  m_sourceid.subsystem = RDT::subsystem;
  m_processing_delay_ticks = mcfg->get_module_configuration()->get_post_processing_delay_ticks();
  

  // Configure implementations:
  m_raw_processor_impl->conf(mcfg);
  // Configure the latency buffer before the request handler so the request handler can check for alignment
  // restrictions
  try {
    m_latency_buffer_impl->conf(mcfg->get_module_configuration()->get_latency_buffer());
  } catch (const std::bad_alloc& be) {
    ers::error(ConfigurationError(ERS_HERE, m_sourceid, "Latency Buffer can't be allocated with size!"));
  }
  m_request_handler_impl->conf(mcfg);
}

template<class RDT, class RHT, class LBT, class RPT>
void 
DataHandlingModel<RDT, RHT, LBT, RPT>::conf(const nlohmann::json& /*args*/)
{
  // Register callbacks if operating in that mode.
  if (m_callback_mode) {
    // Configure and register consume callback
    m_consume_callback = std::bind(&DataHandlingModel<RDT, RHT, LBT, RPT>::consume_payload, this, std::placeholders::_1);
 
    // Register callback
    auto dmcbr = DataMoveCallbackRegistry::get();
    dmcbr->register_callback<RDT>(m_raw_data_receiver_connection_name, m_consume_callback);
  }

  // Configure threads:
  m_consumer_thread.set_name("consumer", m_sourceid.id);
  if (m_generate_timesync)
    m_timesync_thread.set_name("timesync", m_sourceid.id);
}


template<class RDT, class RHT, class LBT, class RPT>
void 
DataHandlingModel<RDT, RHT, LBT, RPT>::start(const nlohmann::json& args)
{
  // Reset opmon variables
  m_sum_payloads = 0;
  m_num_payloads = 0;
  m_sum_requests = 0;
  m_num_requests = 0;
  m_num_payloads_overwritten = 0;
  m_stats_packet_count = 0;
  m_rawq_timeout_count = 0;

  m_t0 = std::chrono::high_resolution_clock::now();

  m_run_number = args.value<dunedaq::daqdataformats::run_number_t>("run", 1);

  TLOG_DEBUG(TLVL_WORK_STEPS) << "Starting threads...";
  m_raw_processor_impl->start(args);
  m_request_handler_impl->start(args);
  if (!m_callback_mode) {
    m_consumer_thread.set_work(&DataHandlingModel<RDT, RHT, LBT, RPT>::run_consume, this);
  }
  if (m_generate_timesync) m_timesync_thread.set_work(&DataHandlingModel<RDT, RHT, LBT, RPT>::run_timesync, this);
  // Register callback to receive and dispatch data requests
  m_data_request_receiver->add_callback(
    std::bind(&DataHandlingModel<RDT, RHT, LBT, RPT>::dispatch_requests, this, std::placeholders::_1));
}

template<class RDT, class RHT, class LBT, class RPT>
void 
DataHandlingModel<RDT, RHT, LBT, RPT>::stop(const nlohmann::json& args)
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Stoppping threads...";

  // Stop receiving data requests as first thing
  m_data_request_receiver->remove_callback();
  // Stop the other threads
  m_request_handler_impl->stop(args);
  if (m_generate_timesync) {
    while (!m_timesync_thread.get_readiness()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  if (!m_callback_mode) {
    while (!m_consumer_thread.get_readiness()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Flushing latency buffer with occupancy: " << m_latency_buffer_impl->occupancy();
  m_latency_buffer_impl->flush();
  m_raw_processor_impl->stop(args);
  m_raw_processor_impl->reset_last_daq_time();
}

template<class RDT, class RHT, class LBT, class RPT>
void 
DataHandlingModel<RDT, RHT, LBT, RPT>::generate_opmon_data()
 {
   opmon::DataHandlerInfo ri;
   ri.set_sum_payloads(m_sum_payloads.load());
   ri.set_num_payloads(m_num_payloads.exchange(0));

   ri.set_num_data_input_timeouts(m_rawq_timeout_count.exchange(0));

   auto now = std::chrono::high_resolution_clock::now();
   int new_packets = m_stats_packet_count.exchange(0);
   double seconds = std::chrono::duration_cast<std::chrono::microseconds>(now - m_t0).count() / 1000000.;
   m_t0 = now;

   ri.set_rate_payloads_consumed(new_packets / seconds / 1000.);
   ri.set_num_payloads_overwritten(m_num_payloads_overwritten.exchange(0));
   ri.set_sum_requests(m_sum_requests.load());
   ri.set_num_requests(m_num_requests.exchange(0));
   ri.set_last_daq_timestamp(m_raw_processor_impl->get_last_daq_time());

   this->publish(std::move(ri));
 }

template<class RDT, class RHT, class LBT, class RPT>
void 
DataHandlingModel<RDT, RHT, LBT, RPT>::run_consume()
{

  TLOG_DEBUG(TLVL_WORK_STEPS) << "Consumer thread started...";
  m_rawq_timeout_count = 0;
  m_num_payloads = 0;
  m_sum_payloads = 0;
  m_stats_packet_count = 0;

  // Variables for book-keeping of delayed post-processing
  //timestamp_t oldest_ts=0;
  timestamp_t newest_ts=0;
  timestamp_t end_win_ts=0;
  bool first_cycle = true;
  auto last_post_proc_time = std::chrono::system_clock::now();
  auto now = last_post_proc_time;
  std::chrono::milliseconds milliseconds;
  RDT processed_element;

  while (m_run_marker.load()) {
    // Try to acquire data

    auto opt_payload = m_raw_data_receiver->try_receive(m_raw_receiver_timeout_ms);

    if (opt_payload) {

      RDT& payload = opt_payload.value();

      m_raw_processor_impl->preprocess_item(&payload);
      if (m_request_handler_supports_cutoff_timestamp) {
        int64_t diff1 = payload.get_timestamp() - m_request_handler_impl->get_cutoff_timestamp();
        if (diff1 <= 0) {
          //m_request_handler_impl->increment_tardy_tp_count();
          ers::warning(DataPacketArrivedTooLate(ERS_HERE, m_run_number, payload.get_timestamp(),
                                                m_request_handler_impl->get_cutoff_timestamp(), diff1,
                                                (static_cast<double>(diff1)/62500.0)));
        }
      }
      if (!m_latency_buffer_impl->write(std::move(payload))) {
        //TLOG_DEBUG(TLVL_TAKE_NOTE) << "***ERROR: Latency buffer is full and data was overwritten!";
        m_num_payloads_overwritten++;
      }
      if (m_processing_delay_ticks ==0) {
        m_raw_processor_impl->postprocess_item(m_latency_buffer_impl->back());
        ++m_num_payloads;
        ++m_sum_payloads;
        ++m_stats_packet_count;
      }
    } else {
      ++m_rawq_timeout_count;
      // Protection against a zero sleep becoming a yield
      if ( m_raw_receiver_sleep_us != std::chrono::microseconds::zero())
        std::this_thread::sleep_for(m_raw_receiver_sleep_us);
    }

    // Add here a possible deferral of the post processing, to allow elements being reordered in the LB
    // Basically, find data older than a certain timestamp and process all data since the last post-processed element up to that value
    if (m_processing_delay_ticks !=0 && m_latency_buffer_impl->occupancy() > 0) {
      now = std::chrono::system_clock::now();
      milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_post_proc_time);

      if (milliseconds.count() > 1) {
        last_post_proc_time = now;
  	// Get the LB boundaries
	auto tail = m_latency_buffer_impl->back();
        newest_ts = tail->get_timestamp();
        
        if (first_cycle) {
	  auto head = m_latency_buffer_impl->front();
          processed_element.set_timestamp(head->get_timestamp()); 
          first_cycle = false;
	  TLOG() << "***** First pass post processing *****" ;
        }
        
	if (newest_ts - processed_element.get_timestamp() > m_processing_delay_ticks) {
    	  end_win_ts = newest_ts - m_processing_delay_ticks; 
	  auto start_iter=m_latency_buffer_impl->lower_bound(processed_element, false);
	  processed_element.set_timestamp(end_win_ts);
	  auto end_iter=m_latency_buffer_impl->lower_bound(processed_element, false);

	  for (auto it = start_iter; it!= end_iter; ++it) { 
              m_raw_processor_impl->postprocess_item(&(*it));
              ++m_num_payloads;
              ++m_sum_payloads;
              ++m_stats_packet_count;
	  }
 	 }
      }
    }
  }
  TLOG_DEBUG(TLVL_WORK_STEPS) << "Consumer thread joins... ";
}

template<class RDT, class RHT, class LBT, class RPT>
void
DataHandlingModel<RDT, RHT, LBT, RPT>::consume_payload(RDT&& payload)
{
 //m_rawq_timeout_count = 0;
 //m_num_payloads = 0;
 //m_sum_payloads = 0;
 //m_stats_packet_count = 0;
  m_raw_processor_impl->preprocess_item(&payload);
  if (m_request_handler_supports_cutoff_timestamp) {
    int64_t diff1 = payload.get_timestamp() - m_request_handler_impl->get_cutoff_timestamp();
    if (diff1 <= 0) {
      //m_request_handler_impl->increment_tardy_tp_count();
      ers::warning(DataPacketArrivedTooLate(ERS_HERE, m_run_number, payload.get_timestamp(),
                                            m_request_handler_impl->get_cutoff_timestamp(), diff1,
                                            (static_cast<double>(diff1)/62500.0)));
    }
  }
  if (!m_latency_buffer_impl->write(std::move(payload))) {
    TLOG_DEBUG(TLVL_TAKE_NOTE) << "***ERROR: Latency buffer is full and data was overwritten!";
    m_num_payloads_overwritten++;
  }
#warning RS FIXME: Post-processing delay feature is not implemented in callback consume!
  m_raw_processor_impl->postprocess_item(m_latency_buffer_impl->back());
  ++m_num_payloads;
  ++m_sum_payloads;
  ++m_stats_packet_count;
}

template<class RDT, class RHT, class LBT, class RPT>
void 
DataHandlingModel<RDT, RHT, LBT, RPT>::run_timesync()
{
  TLOG_DEBUG(TLVL_WORK_STEPS) << "TimeSync thread started...";
  m_num_requests = 0;
  m_sum_requests = 0;
  uint64_t msg_seqno = 0;
  timestamp_t prev_timestamp = 0;
  auto once_per_run = true;
  size_t zero_timestamp_count = 0;
  size_t duplicate_timestamp_count = 0;
  size_t total_timestamp_count = 0;
  while (m_run_marker.load()) {
    try {
      auto timesyncmsg = dfmessages::TimeSync(m_raw_processor_impl->get_last_daq_time());
      ++total_timestamp_count;
      // daq_time is zero for the first received timesync, and may
      // be the same as the previous daq_time if the data has
      // stopped flowing. In both cases we don't send the TimeSync
      if (timesyncmsg.daq_time != 0 && timesyncmsg.daq_time != prev_timestamp) {
        prev_timestamp = timesyncmsg.daq_time;
        timesyncmsg.run_number = m_run_number;
        timesyncmsg.sequence_number = ++msg_seqno;
        timesyncmsg.source_pid = m_pid_of_current_process;
        TLOG_DEBUG(TLVL_TIME_SYNCS) << "New timesync: daq=" << timesyncmsg.daq_time
          << " wall=" << timesyncmsg.system_time << " run=" << timesyncmsg.run_number
          << " seqno=" << timesyncmsg.sequence_number << " pid=" << timesyncmsg.source_pid;
        try {
            dfmessages::TimeSync timesyncmsg_copy(timesyncmsg);
          m_timesync_sender->send(std::move(timesyncmsg_copy), std::chrono::milliseconds(500));
        } catch (ers::Issue& excpt) {
          ers::warning(
            TimeSyncTransmissionFailed(ERS_HERE, m_sourceid, m_timesync_connection_name, excpt));
        }
          
        if (m_fake_trigger) {
          dfmessages::DataRequest dr;
          ++m_current_fake_trigger_id;
          dr.trigger_number = m_current_fake_trigger_id;
          dr.trigger_timestamp = timesyncmsg.daq_time > 500 * us ? timesyncmsg.daq_time - 500 * us : 0;
          auto width = 300000;
          uint offset = 100;
          dr.request_information.window_begin = dr.trigger_timestamp > offset ? dr.trigger_timestamp - offset : 0;
          dr.request_information.window_end = dr.request_information.window_begin + width;
          dr.request_information.component = m_sourceid;
          dr.data_destination = "data_fragments_q";
          TLOG_DEBUG(TLVL_WORK_STEPS) << "Issuing fake trigger based on timesync. "
            << " ts=" << dr.trigger_timestamp 
            << " window_begin=" << dr.request_information.window_begin
            << " window_end=" << dr.request_information.window_end;
          m_request_handler_impl->issue_request(dr);

          ++m_num_requests;
          ++m_sum_requests;
        }
      } else {
        if (timesyncmsg.daq_time == 0) {++zero_timestamp_count;}
        if (timesyncmsg.daq_time == prev_timestamp) {++duplicate_timestamp_count;}
        if (once_per_run) {
          TLOG() << "Timesync with DAQ time 0 won't be sent out as it's an invalid sync.";
          once_per_run = false;
        }
      }
    } catch (const iomanager::TimeoutExpired& excpt) {
      // ++m_timesyncqueue_timeout;
    }
    // Split up the 100ms sleep into 10 sleeps of 10ms, so we respond to "stop" quicker
    for (size_t i=0; i<10; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (!m_run_marker.load()) {
        break;
      }
    }
  }
  once_per_run = true;
  TLOG_DEBUG(TLVL_WORK_STEPS) << "TimeSync thread joins... (timestamp count, zero/same/total  = "
                              << zero_timestamp_count << "/" << duplicate_timestamp_count << "/"
                              << total_timestamp_count << ")";
}

template<class RDT, class RHT, class LBT, class RPT>
void 
DataHandlingModel<RDT, RHT, LBT, RPT>::dispatch_requests(dfmessages::DataRequest& data_request)
{
  if (data_request.request_information.component != m_sourceid) {
     ers::error(RequestSourceIDMismatch(ERS_HERE, m_sourceid, data_request.request_information.component));
     return;
  }
  TLOG_DEBUG(TLVL_QUEUE_POP) << "Received DataRequest" 
    << " for trig/seq_number " << data_request.trigger_number << "." << data_request.sequence_number
    << ", runno " << data_request.run_number
    << ", trig timestamp " << data_request.trigger_timestamp
    << ", SourceID: " << data_request.request_information.component
    << ", window begin/end " << data_request.request_information.window_begin
    << "/" << data_request.request_information.window_end
    << ", dest: " << data_request.data_destination;
  m_request_handler_impl->issue_request(data_request);
  ++m_num_requests;
  ++m_sum_requests;
}

} // namespace datahandlinglibs
} // namespace dunedaq
