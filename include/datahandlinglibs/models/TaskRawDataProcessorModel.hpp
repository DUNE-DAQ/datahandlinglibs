/**
 * @file TaskRawDataProcessorModel.hpp Raw processor parallel task and pipeline
 * combination using a vector of std::functions
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_TASKRAWDATAPROCESSORMODEL_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_TASKRAWDATAPROCESSORMODEL_HPP_

#include "daqdataformats/SourceID.hpp"
#include "logging/Logging.hpp"
#include "datahandlinglibs/FrameErrorRegistry.hpp"
#include "datahandlinglibs/DataHandlingIssues.hpp"
#include "datahandlinglibs/ReadoutLogging.hpp"
#include "datahandlinglibs/concepts/RawDataProcessorConcept.hpp"

#include  "datahandlinglibs/opmon/datahandling_info.pb.h"

#include "confmodel/DaqModule.hpp"
#include "confmodel/Connection.hpp"
#include "appmodel/DataHandlerModule.hpp"
#include "appmodel/DataHandlerConf.hpp"
#include "appmodel/DataProcessor.hpp"

#include "datahandlinglibs/utils/ReusableThread.hpp"

#include <folly/ProducerConsumerQueue.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using dunedaq::datahandlinglibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace datahandlinglibs {

template<class ReadoutType>
class TaskRawDataProcessorModel : public RawDataProcessorConcept<ReadoutType>
{
public:
  // Excplicit constructor with error registry
  explicit TaskRawDataProcessorModel(std::unique_ptr<FrameErrorRegistry>& error_registry, bool post_processing_enabled)
    : RawDataProcessorConcept<ReadoutType>()
    , m_error_registry(error_registry)
	, m_post_processing_enabled(post_processing_enabled)
  {}

  // Destructor
  ~TaskRawDataProcessorModel() {}

  // Configures the element pointer queue for the post-processors, and the SourceID
  void conf(const appmodel::DataHandlerModule* conf) override;

  // Clears elements to process, pre-proc pipeline, and post-proc functions
  void scrap(const nlohmann::json& /*cfg*/) override;

  // Starts the pre-processor pipeline and the parallel post-processor threads
  void start(const nlohmann::json& /*args*/) override;

  // Stops the pre-processor pipeline and the parallel post-processor threads
  void stop(const nlohmann::json& /*args*/) override;

  // Resets last known/processed DAQ timestamp
  void reset_last_daq_time() { m_last_processed_daq_ts.store(0); }

  // Returns last processed ReadoutTyped element's DAQ timestamp
  std::uint64_t get_last_daq_time() override { return m_last_processed_daq_ts.load(); } // NOLINT(build/unsigned)

  // Registers ReadoutType item pointer to the pre-processing pipeline
  void preprocess_item(ReadoutType* item) override { invoke_all_preprocess_functions(item); }

  // Registers ReadoutType item pointer to to the post-processing queue
  void postprocess_item(const ReadoutType* item) override;

  // Registers a pre-processing task to the pre-processor pipeline
  template<typename Task>
  void add_preprocess_task(Task&& task);

  // Registers a post-processing task to the parallel post-processors
  template<typename Task>
  void add_postprocess_task(Task&& task);

  // Invokes all preprocessor functions as pipeline
  void invoke_all_preprocess_functions(ReadoutType* item);

  // Launches all preprocessor functions as async
  void launch_all_preprocess_functions(ReadoutType* item);

protected:
  // Operational monitoring
  virtual void generate_opmon_data() override;

  // Pre-processing pipeline functions
  std::vector<std::function<void(ReadoutType*)>> m_preprocess_functions;
  std::unique_ptr<FrameErrorRegistry>& m_error_registry;

  // Post-processing thread runner
  void run_post_processing_thread(std::function<void(const ReadoutType*)>& function,
                                  folly::ProducerConsumerQueue<const ReadoutType*>& queue);
  bool m_post_processing_enabled;

  // Run marker
  std::atomic<bool> m_run_marker{ false };

  // Post-processing functions and their corresponding threads
  std::vector<std::function<void(const ReadoutType*)>> m_post_process_functions;
  std::vector<std::unique_ptr<folly::ProducerConsumerQueue<const ReadoutType*>>> m_items_to_postprocess_queues;
  std::vector<std::unique_ptr<ReusableThread>> m_post_process_threads;

  // Internals
  size_t m_postprocess_queue_sizes;
  //uint32_t m_this_link_number; // NOLINT(build/unsigned)
  daqdataformats::SourceID m_sourceid;
  //bool m_emulator_mode{ false };
  std::atomic<uint64_t> m_last_processed_daq_ts{ 0 }; // NOLINT(build/unsigned)

};

} // namespace datahandlinglibs
} // namespace dunedaq

// Declarations
#include "detail/TaskRawDataProcessorModel.hxx"

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_TASKRAWDATAPROCESSORMODEL_HPP_
