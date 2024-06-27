/**
 * @file RawDataHandlerBase.hpp Implements standard 
 * module functionalities, requires the setup of a readout
 * creator function. This class is meant to be inherited
 * to specify which readout specialization to load.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_DATALINKHANDLERBASE_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_DATALINKHANDLERBASE_HPP_

#include "datahandlinglibs/DataHandlingIssues.hpp"
#include "datahandlinglibs/concepts/DataHandlingConcept.hpp"
#include "datahandlinglibs/ReadoutLogging.hpp"
#include "daqdataformats/Types.hpp"
#include "logging/Logging.hpp"

#include "nlohmann/json.hpp"
#include "rcif/cmd/Nljs.hpp"

#include "appfwk/ModuleConfiguration.hpp"
#include "appmodel/DataHandlerModule.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace datahandlinglibs {

class RawDataHandlerBase
{
public:
  /**
   * @brief RawDataHandlerBase Constructor
   * @param name Instance name for this RawDataHandlerBase instance
   */
  explicit RawDataHandlerBase(const std::string& name);
  virtual ~RawDataHandlerBase() {}

  RawDataHandlerBase(const RawDataHandlerBase&) = delete;            ///< RawDataHandlerBase is not copy-constructible
  RawDataHandlerBase& operator=(const RawDataHandlerBase&) = delete; ///< RawDataHandlerBase is not copy-assignable
  RawDataHandlerBase(RawDataHandlerBase&&) = delete;                 ///< RawDataHandlerBase is not move-constructible
  RawDataHandlerBase& operator=(RawDataHandlerBase&&) = delete;      ///< RawDataHandlerBase is not move-assignable

  void init(std::shared_ptr<appfwk::ModuleConfiguration> cfg);
  void get_info(opmonlib::InfoCollector& ci, int level);

  virtual std::unique_ptr<dunedaq::datahandlinglibs::DataHandlingConcept>
  create_readout(const appmodel::DataHandlerModule* modconf, std::atomic<bool>& run_marker) = 0;

  // Commands
  void do_conf(const nlohmann::json& /*args*/);
  void do_scrap(const nlohmann::json& /*args*/);
  void do_start(const nlohmann::json& /*args*/);
  void do_stop(const nlohmann::json& /*args*/);
  void do_record(const nlohmann::json& /*args*/);

  std::string get_dlh_name() { return m_name; }

private:

  // Configuration
  bool m_configured;
  daqdataformats::run_number_t m_run_number;

  // Name
  std::string m_name;

  // Internal
  std::unique_ptr<datahandlinglibs::DataHandlingConcept> m_readout_impl;

  // Threading
  std::atomic<bool> m_run_marker;
};

} // namespace datahandlinglibs
} // namespace dunedaq

// Declarations
#include "detail/RawDataHandlerBase.hxx"

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_DATALINKHANDLERBASE_HPP_
