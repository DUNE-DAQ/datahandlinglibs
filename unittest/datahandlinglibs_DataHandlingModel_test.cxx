/**
 * @file datahandlinglibs_DataHandlingModel_test.cxx Unit Tests for DataHandlingModel
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#define BOOST_TEST_MODULE datahandlinglibs_DataHandlingModel_test // NOLINT

#include "boost/test/unit_test.hpp"
#include "datahandlinglibs/models/DataHandlingModel.hpp"
#include "datahandlinglibs/opmon/datahandling_info.pb.h"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"
#include "datahandlinglibs/testutils/UnitTestUtilities.hpp"
#include "datahandlinglibs/models/IterableQueueModel.hpp"
#include "datahandlinglibs/models/BinarySearchQueueModel.hpp"
#include "datahandlinglibs/models/FixedRateQueueModel.hpp"


#include <folly/coro/BlockingWait.h>
#include <folly/coro/Timeout.h>



using namespace dunedaq::datahandlinglibs;

BOOST_AUTO_TEST_SUITE(datahandlinglibs_DataHandlingModel_test)

using RDT = unittest::FakeReadoutType;
using RHT = unittest::FakeRequestHandlerType<unittest::FakeReadoutType,unittest::FakeLatencyBufferType<unittest::FakeReadoutType>>;
using LBT = unittest::FakeLatencyBufferType<unittest::FakeReadoutType>;
using RPT = unittest::FakeRawDataProcessorType;

template<
    typename ReadoutType,typename RequestHandlerType,typename LatencyBufferType,typename RawDataProcessorType,typename InputDataType = ReadoutType> 
    class Test_Handling_Model : public DataHandlingModel<ReadoutType,
                                                    RequestHandlerType,
                                                    LatencyBufferType,
                                                    RawDataProcessorType,
                                                    InputDataType>
{
    using Base = DataHandlingModel<ReadoutType,RequestHandlerType,LatencyBufferType,RawDataProcessorType,InputDataType>;

public:
    // Inherit all constructors from the base class
    using Base::Base;

    void initialize(bool post_pro_enabled)
    {
        this->m_error_registry.reset(new FrameErrorRegistry());
        this->m_error_registry->set_ers_metadata("DLH of SourceID[" + std::to_string(1) + "] ");
        this->m_latency_buffer_impl.reset(new LatencyBufferType());
        this->m_raw_processor_impl.reset(new RawDataProcessorType(this->m_error_registry, post_pro_enabled));
        this->m_request_handler_impl.reset(new RequestHandlerType(this->m_latency_buffer_impl, this->m_error_registry));     
    }

    void initialize_iterable_queue(bool post_pro_enabled, int size)
    {
        this->m_error_registry.reset(new FrameErrorRegistry());
        this->m_error_registry->set_ers_metadata("DLH of SourceID[" + std::to_string(1) + "] ");
        this->m_latency_buffer_impl.reset(new LatencyBufferType(size));
        this->m_raw_processor_impl.reset(new RawDataProcessorType(this->m_error_registry, post_pro_enabled));
        this->m_request_handler_impl.reset(new RequestHandlerType(this->m_latency_buffer_impl, this->m_error_registry));     
    }

    void raw_processor(bool & pre_func_one_called, bool & post_func_one_called, int cutoff_timestamp)
    {
        this->m_raw_processor_impl->add_preprocess_task([&pre_func_one_called](ReadoutType* /*elem*/) { pre_func_one_called = true;});
        this->m_raw_processor_impl->add_postprocess_task([&post_func_one_called](const ReadoutType* /*elem*/) { post_func_one_called = true;});
        this->m_request_handler_supports_cutoff_timestamp = true;
        this->m_processing_delay_ticks=0;
        this->m_request_handler_impl->set_cutoff_timestamp(cutoff_timestamp);
        this->m_raw_processor_impl->make_queues();
    }

    void post_schedule_init (bool & pre_func_one_called, bool & post_func_one_called, int cutoff_timestamp,int min_wait)
    {
        raw_processor(pre_func_one_called, post_func_one_called, cutoff_timestamp);

        this->m_postprocess_scheduler_thread.set_name("pprocsched", 1);
        this->m_timekeeper = std::make_unique<folly::ThreadWheelTimekeeper>();

        this->m_processing_delay_ticks=4;
        this->m_post_processing_delay_max_wait=100;
        this->m_num_post_processing_delay_max_waits=0;
        this->m_post_processing_delay_min_wait=min_wait;
        
    }

    void public_process_item(RDT&& payload){this->process_item(std::move(payload));}
    
    template <class IDT> 
    std::vector<RDT> public_transform_payload(IDT& payload) 
    {
        return this->transform_payload(payload);
    }
    
    template <class IDT> void public_transform_and_process(IDT&& payload){ this->transform_and_process(std::forward<IDT>(payload));}
    template <class IDT> void public_consume_callback(IDT&& payload){this->consume_callback(std::move(payload));}
    void public_run_postprocess_scheduler() {
        this->run_postprocess_scheduler();  
    }



    int get_m_num_payloads() {return (this->m_num_payloads).load();}
    int get_m_sum_payloads() {return (this->m_sum_payloads).load();}
    int get_m_stats_packet_count() {return (this->m_stats_packet_count).load();}
    int get_raw_processor_queue_size(){return this->m_raw_processor_impl->get_post_queues_size();}
    int get_m_num_post_processing_delay_max_waits(){return this->m_num_post_processing_delay_max_waits.load();}
    int get_m_num_lb_insert_failures() {return static_cast<int>(this->m_num_lb_insert_failures.load());}

    void change_m_run_marker(bool change){this->m_run_marker = change;}


};



BOOST_AUTO_TEST_CASE(DataHandlingModel_process_item_pre_post_process)
{
    std::atomic<bool> run_marker = true;

    auto model = Test_Handling_Model<RDT,RHT,LBT,RPT,RDT>(run_marker);

    bool pre_func_one_called = false;
    bool post_func_one_called = false;
    model.initialize(true);
    model.raw_processor(pre_func_one_called,post_func_one_called,0);

    RDT elem;
    elem.set_timestamp(2);

    model.public_process_item(std::move(elem));
    BOOST_REQUIRE(pre_func_one_called);
    BOOST_REQUIRE_EQUAL(model.get_m_num_payloads() , 1);
    BOOST_REQUIRE_EQUAL(model.get_m_sum_payloads() , 1);
    BOOST_REQUIRE_EQUAL(model.get_m_stats_packet_count() , 1);

}

BOOST_AUTO_TEST_CASE(DataHandlingModel_process_item_cutoff_triggered)
{
    std::atomic<bool> run_marker = true;

    auto model = Test_Handling_Model<RDT,RHT,LBT,RPT,RDT>(run_marker);

    bool pre_func_one_called = false;
    bool post_func_one_called = false;
    model.initialize(true);
    model.raw_processor(pre_func_one_called,post_func_one_called,10);

    RDT elem;
    elem.set_timestamp(2);

    setenv("DUNEDAQ_ERS_WARNING", "throw", 1);
    BOOST_CHECK_THROW(model.public_process_item(std::move(elem)), std::exception );
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_transform_payload)
{
    std::atomic<bool> run_marker = true;

    auto model = Test_Handling_Model<RDT,RHT,LBT,RPT,unittest::FakeSuperChunkReadoutType>(run_marker);

    unittest::FakeSuperChunkReadoutType input;
    input.set_timestamp(4);
    auto output = model.public_transform_payload(input);
    BOOST_REQUIRE((std::is_same_v<unittest::FakeReadoutType, decltype(output)::value_type>));
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_transform_and_process_same_type)
{
    std::atomic<bool> run_marker = true;

    auto model = Test_Handling_Model<RDT,RHT,LBT,RPT,RDT>(run_marker);
    model.initialize(true);

    bool pre_func_one_called = false;
    bool post_func_one_called = false;
    model.raw_processor(pre_func_one_called,post_func_one_called,0);

    RDT input;
    input.set_timestamp(4);
    model.public_transform_and_process(std::move(input));
    BOOST_REQUIRE(pre_func_one_called);
    BOOST_REQUIRE_EQUAL(model.get_m_num_payloads() , 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_transform_and_process_different_type)
{
    std::atomic<bool> run_marker = true;

    auto model = Test_Handling_Model<RDT,RHT,LBT,RPT,unittest::FakeSuperChunkReadoutType>(run_marker);
    model.initialize(true);

    bool pre_func_one_called = false;
    bool post_func_one_called = false;
    model.raw_processor(pre_func_one_called,post_func_one_called,0);

    unittest::FakeSuperChunkReadoutType input;
    input.set_timestamp(4);
    model.public_transform_and_process(std::move(input));
    BOOST_REQUIRE(pre_func_one_called);
    BOOST_REQUIRE_EQUAL(model.get_m_num_payloads() , 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_consume_callback)
{
    std::atomic<bool> run_marker = true;

    auto model = Test_Handling_Model<RDT,RHT,LBT,RPT,unittest::FakeSuperChunkReadoutType>(run_marker);
    model.initialize(true);

    bool pre_func_one_called = false;
    bool post_func_one_called = false;
    model.raw_processor(pre_func_one_called,post_func_one_called,0);

    unittest::FakeSuperChunkReadoutType input;
    input.set_timestamp(4);
    model.public_consume_callback(std::move(input));
    BOOST_REQUIRE(pre_func_one_called);
    BOOST_REQUIRE_EQUAL(model.get_m_num_payloads() , 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_consume_callback_write_failure)
{
    std::atomic<bool> run_marker = true;

    auto model = Test_Handling_Model<RDT,
                                    unittest::FakeRequestHandlerType<RDT,BinarySearchQueueModel<RDT>>,
                                    BinarySearchQueueModel<RDT>,
                                    RPT,
                                    RDT>(run_marker);
    model.initialize(true); //default size of LB is 2

    bool pre_func_one_called = false;
    bool post_func_one_called = false;
    model.raw_processor(pre_func_one_called,post_func_one_called,0);

    for (int i = 100; i < 102; i++)
    {
        RDT input;
        input.set_timestamp(i);
        model.public_consume_callback(std::move(input));
    }

    BOOST_REQUIRE(pre_func_one_called);
    BOOST_REQUIRE_EQUAL(model.get_m_num_lb_insert_failures() , 1);
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_postprocess_schedule_SkipListLatencyBufferModel)
{
    std::atomic<bool> run_marker = true;

    auto model = Test_Handling_Model<RDT,
                                    unittest::FakeRequestHandlerType<unittest::FakeReadoutType,SkipListLatencyBufferModel<unittest::FakeReadoutType>>,
                                    SkipListLatencyBufferModel<unittest::FakeReadoutType>,
                                    RPT,
                                    RDT>(run_marker);
    model.initialize(true);

    bool pre_func_one_called = false;
    bool post_func_one_called = false;
    model.post_schedule_init (pre_func_one_called,post_func_one_called, 0 ,0);
    
    for (int i = 100; i < 105; i++) {
        RDT input;
        input.set_timestamp(i);
        model.public_transform_and_process(std::move(input));
    }
    //std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::thread postprocess([&]() {
        model.public_run_postprocess_scheduler();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    for (int i = 105; i < 112; i++) 
    {
        RDT input;
        input.set_timestamp(i);
        model.public_transform_and_process(std::move(input));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    model.change_m_run_marker(false);
    {
        RDT input;
        input.set_timestamp(112);
        model.public_transform_and_process(std::move(input));
    }    
    postprocess.join();

    BOOST_REQUIRE_EQUAL(model.get_m_num_post_processing_delay_max_waits() , 0);
    BOOST_REQUIRE_EQUAL(model.get_m_num_payloads() , 8); //newest ts - delay ticks
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_postprocess_schedule_BinarySearchQueueModel)
{
    std::atomic<bool> run_marker = true;

    auto model = Test_Handling_Model<RDT,
                                    unittest::FakeRequestHandlerType<RDT,BinarySearchQueueModel<RDT>>,
                                    BinarySearchQueueModel<RDT>,
                                    RPT,
                                    RDT>(run_marker);
    model.initialize_iterable_queue(true,32);

    bool pre_func_one_called = false;
    bool post_func_one_called = false;
    model.post_schedule_init (pre_func_one_called,post_func_one_called, 0 ,0);
    
    for (int i = 100; i < 105; i++) {
        RDT input;
        input.set_timestamp(i);
        model.public_transform_and_process(std::move(input));
    }
    //std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::thread postprocess([&]() {
        model.public_run_postprocess_scheduler();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    for (int i = 105; i < 112; i++) 
    {
        RDT input;
        input.set_timestamp(i);
        model.public_transform_and_process(std::move(input));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    model.change_m_run_marker(false);
    {
        RDT input;
        input.set_timestamp(112);
        model.public_transform_and_process(std::move(input));
    }    
    postprocess.join();

    BOOST_REQUIRE_EQUAL(model.get_m_num_post_processing_delay_max_waits() , 0);
    BOOST_REQUIRE_EQUAL(model.get_m_num_payloads() , 8); //newest ts - delay ticks
}

BOOST_AUTO_TEST_CASE(DataHandlingModel_postprocess_schedule_FixedRateQueueModel)
{
    std::atomic<bool> run_marker = true;

    auto model = Test_Handling_Model<RDT,
                                    unittest::FakeRequestHandlerType<RDT,FixedRateQueueModel<RDT>>,
                                    FixedRateQueueModel<RDT>,
                                    RPT,
                                    RDT>(run_marker);
    model.initialize_iterable_queue(true,32);

    bool pre_func_one_called = false;
    bool post_func_one_called = false;
    model.post_schedule_init (pre_func_one_called,post_func_one_called, 0 ,0);
    
    for (int i = 100; i < 105; i++) {
        RDT input;
        input.set_timestamp(i);
        model.public_transform_and_process(std::move(input));
    }
    //std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::thread postprocess([&]() {
        model.public_run_postprocess_scheduler();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    for (int i = 105; i < 112; i++) 
    {
        RDT input;
        input.set_timestamp(i);
        model.public_transform_and_process(std::move(input));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    model.change_m_run_marker(false);
    {
        RDT input;
        input.set_timestamp(112);
        model.public_transform_and_process(std::move(input));
    }    
    postprocess.join();

    BOOST_REQUIRE_EQUAL(model.get_m_num_post_processing_delay_max_waits() , 0);
    BOOST_REQUIRE_EQUAL(model.get_m_num_payloads() , 8); //newest ts - delay ticks
}

BOOST_AUTO_TEST_SUITE_END()