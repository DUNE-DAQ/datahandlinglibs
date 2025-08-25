/**
 * @file datahandlinglibs_DefaultRequestHandlerModel_test.cxx Unit Tests for DefaultRequestHandlerModel
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#define BOOST_TEST_MODULE datahandlinglibs_DefaultRequestHandlerModel_test // NOLINT

#include "boost/test/unit_test.hpp"
#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/opmon/datahandling_info.pb.h"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"
#include "datahandlinglibs/testutils/UnitTestUtilities.hpp"

#include "dfmessages/Types.hpp"

#include "dfmessages/DataRequest.hpp"
#include <daqdataformats/FragmentHeader.hpp>

using namespace dunedaq::datahandlinglibs;

BOOST_AUTO_TEST_SUITE(datahandlinglibs_DefaultRequestHandlerModel_test)

using Base = DefaultRequestHandlerModel<unittest::FakeReadoutType, unittest::FakeLatencyBufferType<unittest::FakeReadoutType>>;

class TestHandler : public Base
{
public:
    using Base::Base;

    int get_m_handled_requests () {return (this->m_handled_requests).load();}
    int get_m_response_time_acc () {return (this->m_response_time_acc).load();}
    int get_m_response_time_min () {return (this->m_response_time_min).load();}
    int get_m_response_time_max () {return (this->m_response_time_max).load();}
    int get_m_num_buffer_cleanups () {return (this->m_num_buffer_cleanups).load();}
    int get_m_pops_count () {return (this->m_pops_count).load();}
    int get_m_occupancy () {return (this->m_occupancy).load();}

    void test_start()
    {
        this->m_request_handler_thread_pool = std::make_unique<boost::asio::thread_pool>(1);
        this->m_fragment_send_timeout_ms=100;

        this->m_pop_limit_size=5;
        this->m_pop_size_pct=0.5;
    }

};


dunedaq::dfmessages::DataRequest create_request(int req_number){
    dunedaq::dfmessages::DataRequest req;

    req.request_number = req_number; // Unique number for this request
    req.trigger_number = req_number; // Trigger ID that caused this
    req.run_number = req_number;       // The current run being taken
    req.trigger_timestamp = req_number; // When the event occurred
    req.readout_type = dunedaq::dfmessages::ReadoutType::kInvalid; // Readout mode
    req.sequence_number = req_number;   // Sequence within this run
    req.data_destination = "somewhere"; // Where to send the result

    req.request_information.window_begin = req_number;
    req.request_information.window_end = req_number+1;
    return req;
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_timestamp_virtuals)
{
    auto latency_buffer = std::make_shared<unittest::FakeLatencyBufferType<unittest::FakeReadoutType>>();
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();

    DefaultRequestHandlerModel<unittest::FakeReadoutType, unittest::FakeLatencyBufferType<unittest::FakeReadoutType>> req_handler(latency_buffer, error_registry);

    BOOST_REQUIRE_EQUAL(req_handler.get_cutoff_timestamp(),0);
    BOOST_REQUIRE(!req_handler.supports_cutoff_timestamp());
}

//create_fragment_header,issue_request, data_request
BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_issue_request_empty_buffer)
{
    dunedaq::dfmessages::DataRequest req_1 = create_request(1);

    auto latency_buffer = std::make_shared<unittest::FakeLatencyBufferType<unittest::FakeReadoutType>>();
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
   
    TestHandler testhandler(latency_buffer, error_registry);
    testhandler.test_start();

    testhandler.issue_request(req_1,false);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    BOOST_REQUIRE_EQUAL(testhandler.get_m_handled_requests(),0);
    BOOST_REQUIRE(testhandler.get_m_response_time_acc()==0);
    BOOST_REQUIRE_EQUAL(testhandler.get_m_response_time_min(),std::numeric_limits<int>::max() );
    BOOST_REQUIRE_EQUAL(testhandler.get_m_response_time_max(),0);

}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_issue_request_non_empty_buffer)
{
    auto latency_buffer = std::make_shared<unittest::FakeLatencyBufferType<unittest::FakeReadoutType>>();
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    TestHandler testhandler(latency_buffer, error_registry);

    testhandler.test_start();
    unittest::FakeReadoutType elem;
    elem.set_timestamp(2);
    latency_buffer->write(std::move(elem));
    testhandler.issue_request(create_request(1));

    auto start = std::chrono::steady_clock::now();
    while (testhandler.get_m_handled_requests() < 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(6)) {
            BOOST_FAIL("Timeout: handler never processed the request");
        }
    }
    BOOST_REQUIRE_EQUAL(testhandler.get_m_handled_requests(),1);
    BOOST_REQUIRE(testhandler.get_m_response_time_acc()>0);
    BOOST_REQUIRE_EQUAL(testhandler.get_m_response_time_acc(),testhandler.get_m_response_time_min());
    BOOST_REQUIRE_EQUAL(testhandler.get_m_response_time_acc(),testhandler.get_m_response_time_max());

}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_issue_request_multiple_elem_buffer)
{
    auto latency_buffer = std::make_shared<unittest::FakeLatencyBufferType<unittest::FakeReadoutType>>();
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    TestHandler testhandler(latency_buffer, error_registry);

    testhandler.test_start();
    unittest::FakeReadoutType elem;
    elem.set_timestamp(1);
    latency_buffer->write(std::move(elem));

    unittest::FakeReadoutType elem2;
    elem2.set_timestamp(2);
    latency_buffer->write(std::move(elem2));
    unittest::FakeReadoutType elem3;
    elem3.set_timestamp(4);
    latency_buffer->write(std::move(elem3));

    testhandler.issue_request(create_request(2));

    auto start = std::chrono::steady_clock::now();
    while (testhandler.get_m_handled_requests() < 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(6)) {
            BOOST_FAIL("Timeout: handler never processed the request");
        }
    }
    BOOST_REQUIRE_EQUAL(testhandler.get_m_handled_requests(),1);
    BOOST_REQUIRE(testhandler.get_m_response_time_acc()>0);
    BOOST_REQUIRE_EQUAL(testhandler.get_m_response_time_acc(),testhandler.get_m_response_time_min());
    BOOST_REQUIRE_EQUAL(testhandler.get_m_response_time_acc(),testhandler.get_m_response_time_max());
}

BOOST_AUTO_TEST_CASE(DefaultRequestHandlerModel_cleanup_check)
{
    auto latency_buffer = std::make_shared<unittest::FakeLatencyBufferType<unittest::FakeReadoutType>>();
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    TestHandler testhandler(latency_buffer, error_registry);

    testhandler.test_start();

    for(int i = 0; i<6; i++)
    {
        unittest::FakeReadoutType elem;
        elem.set_timestamp(i);
        latency_buffer->write(std::move(elem));
    }
    
    testhandler.cleanup_check();

    BOOST_REQUIRE_EQUAL(testhandler.get_m_num_buffer_cleanups(),1);
    BOOST_REQUIRE_EQUAL(testhandler.get_m_occupancy(),3);
    BOOST_REQUIRE_EQUAL(testhandler.get_m_pops_count(),3);
    
}

BOOST_AUTO_TEST_SUITE_END()