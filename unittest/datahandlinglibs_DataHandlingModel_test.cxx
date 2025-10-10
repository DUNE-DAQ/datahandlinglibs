/**
 * @file datahandlinglibs_DataHandlingModel_test.cxx Unit Tests for DataHandlingModel
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#define BOOST_TEST_MODULE datahandlinglibs_DataHandlingModel_test // NOLINT

#include "boost/test/unit_test.hpp"

#include "datahandlinglibs/testutils/UnitTestUtilities.hpp"
#include "datahandlinglibs/ReadoutTypes.hpp"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"

BOOST_AUTO_TEST_SUITE(datahandlinglibs_DataHandlingModel_test)

using namespace dunedaq::datahandlinglibs;

using ReadoutType = types::DUMMY_FRAME_STRUCT;

BOOST_AUTO_TEST_CASE(DataHandlingModel_postprocess_schedule_SkipListLatencyBufferModel)
{
  std::atomic<bool> run_marker = true;

  auto model = unittest::MockDataHandlingModel<
    ReadoutType,
    DefaultRequestHandlerModel<ReadoutType, SkipListLatencyBufferModel<ReadoutType>>,
    SkipListLatencyBufferModel<ReadoutType>,
    TaskRawDataProcessorModel<ReadoutType>>(run_marker);

  auto buffer = std::make_shared<SkipListLatencyBufferModel<ReadoutType>>();

  for (int i = 1; i < 6; i++) {
    ReadoutType frame;
    frame.timestamp = i * 62500;
    buffer->write(std::move(frame));
  }

  const bool post_processing_enabled = true;
  auto error_registry = std::make_unique<FrameErrorRegistry>();
  
  auto raw_processor = 
    std::make_shared<TaskRawDataProcessorModel<ReadoutType>>(error_registry, post_processing_enabled);

  const uint64_t delay_ticks = 4 * 62500;
  const uint64_t delay_min_wait = 1;
  const uint64_t delay_max_wait = 2;
  
  typename decltype(model)::PostprocessManager manager{
    *buffer, *raw_processor, delay_ticks, delay_min_wait, delay_max_wait};     
    
  // First pass
  bool timeout = false;
  int processed_count = manager.perform_postprocessing(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 0);

  timeout = true;
  processed_count += manager.perform_postprocessing(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 2);

  processed_count += manager.perform_postprocessing(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 4);
  
  processed_count += manager.perform_postprocessing(timeout);
  BOOST_REQUIRE_EQUAL(processed_count, 5);
}

BOOST_AUTO_TEST_SUITE_END()
