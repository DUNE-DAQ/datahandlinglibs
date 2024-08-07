/**
 * @file DataHandlingConcept.hpp DataHandlingConcept for constructors and
 * forwarding command args.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_CONCEPTS_READOUTCONCEPT_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_CONCEPTS_READOUTCONCEPT_HPP_

#include "appmodel/DataHandlerModule.hpp"
#include "opmonlib/MonitorableObject.hpp"

namespace dunedaq {
namespace datahandlinglibs {

class DataHandlingConcept : public opmonlib::MonitorableObject
{
public:
  DataHandlingConcept() {}
  virtual ~DataHandlingConcept() {}
  DataHandlingConcept(const DataHandlingConcept&) = delete;            ///< DataHandlingConcept is not copy-constructible
  DataHandlingConcept& operator=(const DataHandlingConcept&) = delete; ///< DataHandlingConcept is not copy-assginable
  DataHandlingConcept(DataHandlingConcept&&) = delete;                 ///< DataHandlingConcept is not move-constructible
  DataHandlingConcept& operator=(DataHandlingConcept&&) = delete;      ///< DataHandlingConcept is not move-assignable

  //! Forward calls from the appfwk
  virtual void init(const appmodel::DataHandlerModule* mcfg) = 0;
  virtual void conf(const nlohmann::json& args) = 0;
  virtual void scrap(const nlohmann::json& args) = 0;
  virtual void start(const nlohmann::json& args) = 0;
  virtual void stop(const nlohmann::json& args) = 0;
  virtual void record(const nlohmann::json& args) = 0;

  //! Function that will be run in its own thread to read the raw packets from the connection and add them to the LB
  virtual void run_consume() = 0;
  //! Function that will be run in its own thread and sends periodic timesync messages by pushing them to the connection
  virtual void run_timesync() = 0;
};

} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_CONCEPTS_READOUTCONCEPT_HPP_
