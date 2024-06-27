namespace dunedaq {
namespace datahandlinglibs {

RawDataHandlerBase::RawDataHandlerBase(const std::string& name)
  : m_configured(false)
  , m_name(name)
  , m_readout_impl(nullptr)
  , m_run_marker{ false }
{
/*
  register_command("conf", &RawDataHandlerBase::do_conf);
  register_command("scrap", &RawDataHandlerBase::do_scrap);
  register_command("start", &RawDataHandlerBase::do_start);
  register_command("stop_trigger_sources", &RawDataHandlerBase::do_stop);
  register_command("record", &RawDataHandlerBase::do_record);
*/
}


void
RawDataHandlerBase::init(std::shared_ptr<appfwk::ModuleConfiguration> cfg)
{
  
  TLOG_DEBUG(dunedaq::datahandlinglibs::logging::TLVL_ENTER_EXIT_METHODS) << get_dlh_name() << ": Entering init() method";
  const appmodel::DataHandlerModule* modconf = cfg->module<appmodel::DataHandlerModule>(get_dlh_name());
  if(modconf == nullptr) {
    throw dunedaq::datahandlinglibs::FailedReadoutInitialization(ERS_HERE, get_dlh_name(), "not a DataHandlerModule");
  }
  m_readout_impl = create_readout(modconf, m_run_marker);
  if (m_readout_impl == nullptr) {
    TLOG() << get_dlh_name() << "Initialize readout implementation FAILED! "
           << "Failed to find specialization for given queue setup!";
    throw dunedaq::datahandlinglibs::FailedReadoutInitialization(ERS_HERE, get_dlh_name(), ""); // 4 json ident
  }
  TLOG_DEBUG(dunedaq::datahandlinglibs::logging::TLVL_ENTER_EXIT_METHODS) << get_dlh_name() << ": Exiting init() method";
}

void
RawDataHandlerBase::get_info(opmonlib::InfoCollector& ci, int level)
{
  m_readout_impl->get_info(ci, level);
}

void
RawDataHandlerBase::do_conf(const nlohmann::json& args)
{
  TLOG_DEBUG(dunedaq::datahandlinglibs::logging::TLVL_ENTER_EXIT_METHODS) << get_dlh_name() << ": Entering do_conf() method";
  m_readout_impl->conf(args);
  m_configured = true;
  TLOG_DEBUG(dunedaq::datahandlinglibs::logging::TLVL_ENTER_EXIT_METHODS) << get_dlh_name() << ": Exiting do_conf() method";
}

void
RawDataHandlerBase::do_scrap(const nlohmann::json& args)
{
  TLOG_DEBUG(dunedaq::datahandlinglibs::logging::TLVL_ENTER_EXIT_METHODS) << get_dlh_name() << ": Entering do_scrap() method";
  m_readout_impl->scrap(args);
  m_configured = false;
  TLOG_DEBUG(dunedaq::datahandlinglibs::logging::TLVL_ENTER_EXIT_METHODS) << get_dlh_name() << ": Exiting do_scrap() method";
}
void
RawDataHandlerBase::do_start(const nlohmann::json& args)
{
  TLOG_DEBUG(dunedaq::datahandlinglibs::logging::TLVL_ENTER_EXIT_METHODS) << get_dlh_name() << ": Entering do_start() method";
  m_run_marker.store(true);
  m_readout_impl->start(args);
  rcif::cmd::StartParams start_params = args.get<rcif::cmd::StartParams>();
  m_run_number = start_params.run;
  TLOG() << get_dlh_name() << " successfully started for run number " << m_run_number;
  TLOG_DEBUG(dunedaq::datahandlinglibs::logging::TLVL_ENTER_EXIT_METHODS) << get_dlh_name() << ": Exiting do_start() method";
}

void
RawDataHandlerBase::do_stop(const nlohmann::json& args)
{
  TLOG_DEBUG(dunedaq::datahandlinglibs::logging::TLVL_ENTER_EXIT_METHODS) << get_dlh_name() << ": Entering do_stop() method";
  m_run_marker.store(false);
  m_readout_impl->stop(args);
  TLOG() << get_dlh_name() << " successfully stopped for run number " << m_run_number;
  TLOG_DEBUG(dunedaq::datahandlinglibs::logging::TLVL_ENTER_EXIT_METHODS) << get_dlh_name() << ": Exiting do_stop() method";
}

void
RawDataHandlerBase::do_record(const nlohmann::json& args)
{
  TLOG_DEBUG(dunedaq::datahandlinglibs::logging::TLVL_ENTER_EXIT_METHODS) << get_dlh_name() << ": Entering do_issue_recording() method";
  m_readout_impl->record(args);
  TLOG_DEBUG(dunedaq::datahandlinglibs::logging::TLVL_ENTER_EXIT_METHODS) << get_dlh_name() << ": Exiting do_issue_recording() method";
}

} // namespace datahandlinglibs
} // namespace dunedaq

