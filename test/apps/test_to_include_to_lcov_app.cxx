/**
 * @file test_to_include_to_lcov_app.cxx Test application for
 * ratelimiter implementation
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */


#include "datahandlinglibs/models/BinarySearchQueueModel.hpp"
#include "datahandlinglibs/models/DataHandlingModel.hpp"
#include "datahandlinglibs/models/DataSubscriberModel.hpp"
#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/models/DefaultSkipListRequestHandler.hpp"
#include "datahandlinglibs/models/EmptyFragmentRequestHandlerModel.hpp"
#include "datahandlinglibs/models/FixedRateQueueModel.hpp"
#include "datahandlinglibs/models/IterableQueueModel.hpp"
#include "datahandlinglibs/models/RecorderModel.hpp"

#include "datahandlinglibs/models/SourceEmulatorModel.hpp"
#include "datahandlinglibs/models/TaskRawDataProcessorModel.hpp"
#include "datahandlinglibs/models/ZeroCopyRecordingRequestHandlerModel.hpp"
#include "datahandlinglibs/ReadoutTypes.hpp"




#include "datahandlinglibs/opmon/datahandling_info.pb.h"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"
#include "datahandlinglibs/testutils/UnitTestUtilities.hpp"

#include <cassert>



using namespace dunedaq::datahandlinglibs;
using namespace folly;




int
main(int /*argc*/, char** /*argv[]*/)
{
    
    BinarySearchQueueModel <types::DUMMY_FRAME_STRUCT> binary_search_queue_model;
    std::atomic<bool> flag{true};
    std::atomic<bool> run_marker;

    auto readout_model = std::make_shared<DataHandlingModel<
    unittest::FakeReadoutType,
    unittest::FakeRequestHandlerType,
    unittest::FakeLatencyBufferType<unittest::FakeReadoutType>,
    unittest::FakeRawDataProcessorType,
    int>>(run_marker);

    DataSubscriberModel <types::DUMMY_FRAME_STRUCT> data_subsciber_model;
    auto latency_buffer = std::make_shared<unittest::FakeLatencyBufferType<unittest::FakeReadoutType>>();
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();

    DefaultRequestHandlerModel<unittest::FakeReadoutType, unittest::FakeLatencyBufferType<unittest::FakeReadoutType>> default_request_handler_model(latency_buffer, error_registry);

    auto latency_buffer2 = std::make_shared<SkipListLatencyBufferModel<unittest::FakeReadoutType>>();
    auto error_registry2 = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    DefaultSkipListRequestHandler <unittest::FakeReadoutType> default_skiplist_request_handler(latency_buffer2,error_registry2);
    
    EmptyFragmentRequestHandlerModel <unittest::FakeReadoutType,unittest::FakeLatencyBufferType<unittest::FakeReadoutType>>empty_fragment_request_handler_model(latency_buffer,error_registry);

    FixedRateQueueModel <int> fixed_rate_queue_model;
    IterableQueueModel<int> iterable_queue_model;
    RecorderModel <types::DUMMY_FRAME_STRUCT>recorder_model("name");
    SkipListLatencyBufferModel <types::DUMMY_FRAME_STRUCT> skiplist_latency_buffer_model;
    SourceEmulatorPatternGenerator source_emulator_pattern_generator;
    TaskRawDataProcessorModel <unittest::FakeReadoutType> task_rawdata_processor_model(error_registry,false);
    ZeroCopyRecordingRequestHandlerModel <unittest::FakeReadoutType,unittest::FakeLatencyBufferType<unittest::FakeReadoutType>> zero_copy_recording_request_hander_model(latency_buffer,error_registry);
    



    
    


  return 0;
} // NOLINT(readability/fn_size)
