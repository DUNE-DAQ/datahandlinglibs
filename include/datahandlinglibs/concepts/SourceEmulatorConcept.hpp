/**
 * @file SourceEmulatorConcept.hpp SourceEmulatorConcept for constructors and
 * forwarding command args.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_CONCEPTS_SOURCEEMULATORCONCEPT_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_CONCEPTS_SOURCEEMULATORCONCEPT_HPP_

#include "opmonlib/InfoCollector.hpp"

#include "datahandlinglibs/utils/RateLimiter.hpp"
#include "confmodel/DetectorStream.hpp"
#include "appmodel/StreamEmulationParameters.hpp"

#include <map>
#include <string>
#include <thread>

namespace dunedaq {
namespace datahandlinglibs {

class SourceEmulatorConcept
{
public:
  SourceEmulatorConcept() {}

  virtual ~SourceEmulatorConcept() {}
  SourceEmulatorConcept(const SourceEmulatorConcept&) = delete; ///< SourceEmulatorConcept is not copy-constructible
  SourceEmulatorConcept& operator=(const SourceEmulatorConcept&) =
    delete;                                                ///< SourceEmulatorConcept is not copy-assginable
  SourceEmulatorConcept(SourceEmulatorConcept&&) = delete; ///< SourceEmulatorConcept is not move-constructible
  SourceEmulatorConcept& operator=(SourceEmulatorConcept&&) = delete; ///< SourceEmulatorConcept is not move-assignable

  //virtual void init(const nlohmann::json& /*args*/) = 0;
  virtual void set_sender(const std::string& /*sink_name*/) = 0;
  virtual void conf(const confmodel::DetectorStream* conf, const appmodel::StreamEmulationParameters* emu_conf) = 0;
  virtual void start(const nlohmann::json& /*args*/) = 0;
  virtual void stop(const nlohmann::json& /*args*/) = 0;
  virtual void scrap(const nlohmann::json& /*args*/) = 0;
  virtual void get_info(opmonlib::InfoCollector& ci, int level) = 0;
  virtual bool is_configured() = 0;

private:
};

} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_CONCEPTS_SOURCEEMULATORCONCEPT_HPP_
