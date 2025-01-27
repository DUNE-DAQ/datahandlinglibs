/**
 * @file RecorderModel.hpp Templated recorder implementation
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_RECORDERMODEL_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_RECORDERMODEL_HPP_

#include "iomanager/IOManager.hpp"
#include "iomanager/Receiver.hpp"
#include "utilities/WorkerThread.hpp"
#include "datahandlinglibs/ReadoutTypes.hpp"
#include "datahandlinglibs/concepts/RecorderConcept.hpp"
#include "datahandlinglibs/utils/BufferedFileWriter.hpp"
#include "utilities/ReusableThread.hpp"

#include "datahandlinglibs/opmon/datahandling_info.pb.h"

#include "confmodel/DaqModule.hpp"
#include "confmodel/Connection.hpp"
#include "appmodel/DataRecorderModule.hpp"
#include "appmodel/DataRecorderConf.hpp"
#include <atomic>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace dunedaq {
namespace datahandlinglibs {

template<class ReadoutType>
class RecorderModel : public RecorderConcept
{
public:
  explicit RecorderModel(std::string name)
    : m_work_thread(0)
    , m_name(name)
  {}

  void init(const appmodel::DataRecorderModule* conf) override;
  //  void get_info(opmonlib::InfoCollector& ci, int /* level */) override;
  void do_conf(const nlohmann::json& /*args*/) override;
  void do_scrap(const nlohmann::json& /*args*/) override { m_buffered_writer.close(); }
  void do_start(const nlohmann::json& /* args */) override;
  void do_stop(const nlohmann::json& /* args */) override;

protected:
  virtual void generate_opmon_data() override;

private:
  // The work that the worker thread does
  void do_work();

  // Queue
  using source_t = dunedaq::iomanager::ReceiverConcept<ReadoutType>;
  std::shared_ptr<source_t> m_data_receiver;

  // Internal
  //recorderconfig::Conf m_conf;
  std::string m_output_file;
  size_t m_stream_buffer_size = 0;
  std::string m_compression_algorithm;
  bool m_use_o_direct;

  BufferedFileWriter<> m_buffered_writer;

  // Threading
  utilities::ReusableThread m_work_thread;
  std::atomic<bool> m_run_marker;

  // Stats
  std::atomic<int> m_bytes_processed{ 0 };
  std::atomic<int> m_packets_processed{ 0 };
  std::chrono::steady_clock::time_point m_time_point_last_info;

  std::string m_name;
};

} // namespace datahandlinglibs
} // namespace dunedaq

// Declarations
#include "detail/RecorderModel.hxx"

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_RECORDERMODEL_HPP_
