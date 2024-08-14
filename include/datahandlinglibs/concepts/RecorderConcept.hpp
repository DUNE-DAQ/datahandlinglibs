/**
 * @file RecorderConcept.hpp Recorder concept
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_CONCEPTS_RECORDERCONCEPT_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_CONCEPTS_RECORDERCONCEPT_HPP_

#include "utilities/WorkerThread.hpp"
#include "datahandlinglibs/ReadoutTypes.hpp"
//#include "datahandlinglibs/recorderconfig/Structs.hpp"
#include "datahandlinglibs/utils/BufferedFileWriter.hpp"
#include "datahandlinglibs/utils/ReusableThread.hpp"
#include "appmodel/DataRecorderModule.hpp"

#include <atomic>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace dunedaq {
namespace datahandlinglibs {

class RecorderConcept
{
public:
  RecorderConcept() {}
  virtual ~RecorderConcept() {}
  RecorderConcept(const RecorderConcept&) = delete;
  RecorderConcept& operator=(const RecorderConcept&) = delete;
  RecorderConcept(RecorderConcept&&) = delete;
  RecorderConcept& operator=(RecorderConcept&&) = delete;

  virtual void init(const appmodel::DataRecorderModule* mcfg) = 0;
  
 // Commands
   virtual void do_conf(const nlohmann::json& args) = 0;
  virtual void do_start(const nlohmann::json& obj) = 0;
  virtual void do_stop(const nlohmann::json& obj) = 0;
  virtual void do_scrap(const nlohmann::json& obj) = 0;
};

} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_CONCEPTS_RECORDERCONCEPT_HPP_
