/**
 * @file FakeCardReaderBase.hpp Generate payloads from input file
 * Generates user payloads at a given rate, from raw binary data files.
 * This implementation is purely software based, no I/O devices and tools
 * are needed to use this module.
 *
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_FAKECARDREADERBASE_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_FAKECARDREADERBASE_HPP_

// package
#include "datahandlinglibs/concepts/SourceEmulatorConcept.hpp"

#include "confmodel/DaqModule.hpp"
#include "confmodel/Connection.hpp"
#include "confmodel/QueueWithSourceId.hpp"
#include "confmodel/DetectorToDaqConnection.hpp"
#include "confmodel/DetDataSender.hpp"
#include "confmodel/DetectorStream.hpp"
#include "appmodel/DataReaderModule.hpp"
#include "appmodel/DataReaderConf.hpp"

#include "appfwk/ConfigurationManager.hpp"

#include "datahandlinglibs/utils/ReusableThread.hpp"
#include "datahandlinglibs/utils/FileSourceBuffer.hpp"

#include "nlohmann/json.hpp"
#include "rcif/cmd/Nljs.hpp"

// std
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace datahandlinglibs {

class FakeCardReaderBase
{
public:
  /**
   * @brief FakeCardReaderBase Constructor
   * @param name Instance name for this FakeCardReaderBase instance
   */
  explicit FakeCardReaderBase(const std::string& name);
  virtual ~FakeCardReaderBase() {}

  FakeCardReaderBase(const FakeCardReaderBase&) = delete;            ///< FakeCardReaderBase is not copy-constructible
  FakeCardReaderBase& operator=(const FakeCardReaderBase&) = delete; ///< FakeCardReaderBase is not copy-assignable
  FakeCardReaderBase(FakeCardReaderBase&&) = delete;                 ///< FakeCardReaderBase is not move-constructible
  FakeCardReaderBase& operator=(FakeCardReaderBase&&) = delete;      ///< FakeCardReaderBase is not move-assignable

  void init(std::shared_ptr<appfwk::ConfigurationManager> cfg);

  // To be implemented by final module
  virtual std::shared_ptr<datahandlinglibs::SourceEmulatorConcept>
  create_source_emulator(std::string qi, std::atomic<bool>& run_marker) = 0;

  // Commands
  void do_conf(const nlohmann::json& /*args*/);
  void do_scrap(const nlohmann::json& /*args*/);
  void do_start(const nlohmann::json& /*args*/);
  void do_stop(const nlohmann::json& /*args*/);

  std::string get_fcr_name() { return m_name; }

private:
  // Configuration
  bool m_configured;
  std::string m_name;
  std::shared_ptr<appfwk::ConfigurationManager> m_cfg;

  std::map<std::string, std::shared_ptr<datahandlinglibs::SourceEmulatorConcept>> m_source_emus;

  // Internals
  std::unique_ptr<datahandlinglibs::FileSourceBuffer> m_source_buffer;

  // Threading
  std::atomic<bool> m_run_marker;
};

} // namespace datahandlinglibs
} // namespace dunedaq

// Declarations
#include "detail/FakeCardReaderBase.hxx"

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_FAKECARDREADERBASE_HPP_
