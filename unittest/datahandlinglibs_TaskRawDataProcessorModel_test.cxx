/**
 * @file datahandlinglibs_TaskRawDataProcessorModel_test.cxx Unit Tests for TaskRawDataProcessorModel
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

/**
 * @brief Name of this test module
 */
#define BOOST_TEST_MODULE datahandlinglibs_TaskRawDataProcessorModel_test // NOLINT

#include "boost/test/unit_test.hpp"
#include "datahandlinglibs/models/TaskRawDataProcessorModel.hpp"
#include "datahandlinglibs/opmon/datahandling_info.pb.h"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"
#include "datahandlinglibs/testutils/UnitTestUtilities.hpp"

#include "utilities/ReusableThread.hpp"
#include <folly/ProducerConsumerQueue.h>

#include <stdlib.h>

using namespace dunedaq::datahandlinglibs;

BOOST_AUTO_TEST_SUITE(datahandlinglibs_TaskRawDataProcessorModel_test)


using ROType = unittest::FakeReadoutType;

class Test_Proscessor : public TaskRawDataProcessorModel <ROType> 
{
public:
    using Base = TaskRawDataProcessorModel<ROType>;
    using Base::Base;
    // for decleration:
    //Test_Proscessor<ROType>  processor (error_registry,true);


    void wrap_post_processing_thread(std::function<void(const ROType*)>& func,
                                     folly::ProducerConsumerQueue<const ROType*>& queue)
    {this->run_post_processing_thread(func, queue);}

    void wrap_post_processing_threads() 
    {                           
        for (size_t i = 0; i < this->m_post_process_threads.size(); ++i) {
            this->m_post_process_threads[i]->set_work( &Test_Proscessor::wrap_post_processing_thread ,
            this,
            std::ref(this->m_post_process_functions[i]),
            std::ref(*this->m_items_to_postprocess_queues[i]));
        } 
    }

    void make_queues ()
    {
        for (size_t i = 0; i < this->m_post_process_functions.size(); ++i) {
            this->m_items_to_postprocess_queues.push_back(
            std::make_unique<folly::ProducerConsumerQueue<const ROType*>>(this->m_post_process_functions.size())); //size can be anything
            this->m_post_process_threads[i]->set_name(std::to_string(i), i);
        }
    }
                                  
    //cannot change 
    const std::vector<std::unique_ptr<dunedaq::utilities::ReusableThread>>& get_post_threads(){return this->m_post_process_threads;}  
    std::vector<std::function<void(const ROType*)>>& get_post_functions(){return this->m_post_process_functions;}
    const std::vector<std::unique_ptr<folly::ProducerConsumerQueue<const ROType*>>> & get_post_queues(){return this->m_items_to_postprocess_queues;}
    void run_marker_set (bool m_set) {this->m_run_marker = m_set; }
};

template <typename Type> void pre_func_one([[maybe_unused]] Type* elem) {*elem = (*elem) * (*elem);}
template <typename Type> void pre_func_two([[maybe_unused]] Type* elem) {*elem = (*elem) + 10;}



BOOST_AUTO_TEST_CASE(TaskRawDataProcessorModel_get_last_timestamp)
{
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    TaskRawDataProcessorModel <ROType> processor (error_registry,false);
    processor.reset_last_daq_time();
    BOOST_REQUIRE_EQUAL(processor.get_last_daq_time(), 0);
}

BOOST_AUTO_TEST_CASE(TaskRawDataProcessorModel_invoke_preprocess)
{
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    TaskRawDataProcessorModel <ROType> processor (error_registry,false);
    processor.add_preprocess_task(pre_func_one<ROType>);
    processor.add_preprocess_task(pre_func_two<ROType>);
    
    ROType* pre_pro_result = new ROType();
    ROType* intended_result = new ROType();
    pre_pro_result->set_timestamp(2);
    intended_result->set_timestamp(pre_pro_result->get_timestamp());

    pre_func_one<ROType>(intended_result);
    pre_func_two<ROType>(intended_result);
    processor.invoke_all_preprocess_functions(pre_pro_result);

    BOOST_REQUIRE_EQUAL(*pre_pro_result, *intended_result);

    delete pre_pro_result;
    delete intended_result;
}

BOOST_AUTO_TEST_CASE(TaskRawDataProcessorModel_launch_preprocess)
{
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    TaskRawDataProcessorModel <ROType> processor (error_registry,false);
    bool pre_func_one_called = false; 
    bool pre_func_two_called = false;
    processor.add_preprocess_task([&pre_func_one_called](ROType* /*elem*/) { pre_func_one_called = true;});
    processor.add_preprocess_task([&pre_func_two_called](ROType* /*elem*/) { pre_func_two_called = true;});
    
    ROType* pre_pro_result = new ROType();
    pre_pro_result->set_timestamp(2);
    processor.launch_all_preprocess_functions(pre_pro_result);

    BOOST_REQUIRE(pre_func_one_called);
    BOOST_REQUIRE(pre_func_two_called);

    delete pre_pro_result;
}

BOOST_AUTO_TEST_CASE(TaskRawDataProcessorModel_preprocess_item)
{
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    TaskRawDataProcessorModel <ROType> processor (error_registry,false);
    processor.add_preprocess_task(pre_func_one<ROType>);
    processor.add_preprocess_task(pre_func_two<ROType>);
    
    ROType* pre_pro_result = new ROType();
    ROType* intended_result = new ROType();
    pre_pro_result->set_timestamp(2);
    intended_result->set_timestamp(pre_pro_result->get_timestamp());

    pre_func_one<ROType>(intended_result);
    pre_func_two<ROType>(intended_result);
    processor.preprocess_item(pre_pro_result);

    BOOST_REQUIRE_EQUAL(*pre_pro_result, *intended_result);

    delete pre_pro_result;
    delete intended_result;
}

BOOST_AUTO_TEST_CASE(TaskRawDataProcessorModel_postprocess_add_task)
{
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    Test_Proscessor processor (error_registry,true);
    bool post_func_one_called = false; 
    bool post_func_two_called = false;

    processor.add_postprocess_task([&post_func_one_called](const ROType* /*elem*/) { post_func_one_called = true;});
    processor.add_postprocess_task([&post_func_two_called](const ROType* /*elem*/) { post_func_two_called = true;});
    BOOST_REQUIRE_EQUAL(processor.get_post_threads().size(),2);
    BOOST_REQUIRE_EQUAL(processor.get_post_functions().size(),2);
}

BOOST_AUTO_TEST_CASE(TaskRawDataProcessorModel_postprocess_queue)
{
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    Test_Proscessor processor (error_registry,true);

    bool post_func_one_called = false;
    bool post_func_two_called = false;

    processor.add_postprocess_task([&post_func_one_called](const ROType* /*elem*/) { post_func_one_called = true;});
    processor.add_postprocess_task([&post_func_two_called](const ROType* /*elem*/) { post_func_two_called = true;});
    
    processor.make_queues();
    BOOST_REQUIRE_EQUAL(processor.get_post_queues().size(),2);
}

BOOST_AUTO_TEST_CASE(TaskRawDataProcessorModel_add_to_queue)
{
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    Test_Proscessor processor (error_registry,true);

    bool post_func_one_called = false;
    bool post_func_two_called = false;

    processor.add_postprocess_task([&post_func_one_called](const ROType* /*elem*/) { post_func_one_called = true;});
    processor.add_postprocess_task([&post_func_two_called](const ROType* /*elem*/) { post_func_two_called = true;});
    processor.make_queues();

    ROType* post_pro_elem = new ROType();
    post_pro_elem->set_timestamp(2);
    processor.postprocess_item(post_pro_elem);

    //check size of each queue
    for (unsigned long i = 0; i < processor.get_post_queues().size(); i++){
        BOOST_REQUIRE_EQUAL(processor.get_post_queues()[i]->sizeGuess(), 1);
    }
}

BOOST_AUTO_TEST_CASE(TaskRawDataProcessorModel_postprocess_run_threads_after_queue)
{
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    Test_Proscessor processor (error_registry,true);

    bool post_func_one_called = false;
    bool post_func_two_called = false;

    processor.add_postprocess_task([&post_func_one_called](const ROType* /*elem*/) { post_func_one_called = true;});
    processor.add_postprocess_task([&post_func_two_called](const ROType* /*elem*/) { post_func_two_called = true;});
    processor.make_queues();
    ROType* post_pro_elem = new ROType();
    post_pro_elem->set_timestamp(2);
    processor.postprocess_item(post_pro_elem);

    processor.wrap_post_processing_threads();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // if not used main thread finishes early

    BOOST_REQUIRE(post_func_one_called);
    BOOST_REQUIRE(post_func_two_called);
}

BOOST_AUTO_TEST_CASE(TaskRawDataProcessorModel_postprocess_run_threads_before_queue)
{
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    Test_Proscessor processor (error_registry,true);

    bool post_func_one_called = false;
    bool post_func_two_called = false;

    processor.add_postprocess_task([&post_func_one_called](const ROType* /*elem*/) { post_func_one_called = true;});
    processor.add_postprocess_task([&post_func_two_called](const ROType* /*elem*/) { post_func_two_called = true;});
    processor.make_queues();

    processor.run_marker_set(true); //m_run_marker.load() || queue.sizeGuess() > 0
    processor.wrap_post_processing_threads();

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); //wait before adding elements to the queue
    
    ROType* post_pro_elem = new ROType();
    post_pro_elem->set_timestamp(2);
    processor.postprocess_item(post_pro_elem);

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // if not used main thread finishes early
    processor.run_marker_set(false);

    BOOST_REQUIRE(post_func_one_called);
    BOOST_REQUIRE(post_func_two_called);
    
}

BOOST_AUTO_TEST_CASE(TaskRawDataProcessorModel_postprocess_fail_postprocess_item)
{

    setenv("DUNEDAQ_ERS_WARNING", "throw", 1);

    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    Test_Proscessor processor (error_registry,true);

    bool post_func_one_called = false;
    bool post_func_two_called = false;

    processor.add_postprocess_task([&post_func_one_called](const ROType* /*elem*/) { post_func_one_called = true;});
    processor.add_postprocess_task([&post_func_two_called](const ROType* /*elem*/) { post_func_two_called = true;});
    processor.make_queues(); //size is 2

    ROType* post_pro_elem = new ROType();
    post_pro_elem->set_timestamp(2);
    processor.postprocess_item(post_pro_elem);

    //when queue is full write fails
    ROType* fail_elem = new ROType();
    fail_elem->set_timestamp(4);
    

    BOOST_CHECK_THROW(processor.postprocess_item(fail_elem), std::exception );
    
}

BOOST_AUTO_TEST_SUITE_END()
