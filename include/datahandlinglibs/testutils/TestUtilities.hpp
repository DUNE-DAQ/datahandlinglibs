//
// Created by Wesley Ketchum on 10/28/24.
//

#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_TESTUTILITIES_HPP
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_TESTUTILITIES_HPP

#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/FrameErrorRegistry.hpp"
#include "datahandlinglibs/concepts/LatencyBufferConcept.hpp"

#include "boost/test/unit_test.hpp"

#include <memory>
#include <sstream>
#include <set>
#include <cmath>
#include <iterator>


namespace dunedaq{
namespace datahandlinglibs{
namespace test{

//general test buffer struct that can be reused
template <template<class> class BufferType, class TypeAdapter>
void fill_buffer( std::shared_ptr< BufferType<TypeAdapter> >& buffer,
                  uint64_t const init_timestamp=0,
                  size_t const n_obj=10,
                  std::set<size_t> const obj_to_skip = {}){

    //allocate mem in buffer
    buffer->allocate_memory(n_obj);

    //some accounting
    size_t i=0,i_obj=0;
    uint64_t next_timestamp=init_timestamp;

    //while we want to add to the buffer
    while(i_obj<n_obj){

        //create a frame with appropriate timestamp
        TypeAdapter frame;
        frame.fake_timestamps(next_timestamp);

        //increment to the next timestamp
        next_timestamp += uint64_t(frame.get_num_frames()*TypeAdapter::expected_tick_difference);

        //only write in buffer if not in the skip list
        if(obj_to_skip.count(i)==0) {
            buffer->write(std::move(frame));
            ++i_obj;
        }
        ++i;
    } // end while(obj_in_buffer<n_obj)
}//end fill_buffer

template <template<class> class BufferType, class TypeAdapter>
void print_buffer(std::shared_ptr<BufferType<TypeAdapter>>& buffer, std::string desc=""){
    std::stringstream ss;
    ss << "Buffer (" << desc << "): ";
    typename BufferType<TypeAdapter>::Iterator iter=buffer->begin();
    while(iter!=buffer->end()){
        ss << iter->get_timestamp() << " ";
        ++iter;
    }
    BOOST_TEST_MESSAGE(ss.str());
}//end print_buffer

template <template<class> class BufferType, class TypeAdapter>
void test_queue_model()
{
    //some testing vars
    TypeAdapter test_element;
    uint64_t ticks_between = TypeAdapter::expected_tick_difference*test_element.get_num_frames();

    auto test_lower_bound = [&]( std::shared_ptr<BufferType<TypeAdapter>> buffer,
                                 uint64_t test_ts,
                                 uint32_t expected_idx,
                                 bool with_errors=false){

        TypeAdapter test_element; test_element.set_timestamp(test_ts);
        typename BufferType<TypeAdapter>::Iterator expected_el =  buffer->begin();
        for(size_t i=0; i<expected_idx; ++i){
            ++expected_el;
        }

        //get our lower_bound call
        auto return_el = buffer->lower_bound(test_element,with_errors);

        //loop through to get the previous element to return_el
        typename BufferType<TypeAdapter>::Iterator scan_el =  buffer->begin();
        typename BufferType<TypeAdapter>::Iterator prev_el =  buffer->begin();
        while(scan_el!=return_el){
            ++scan_el;
            if(scan_el==return_el) break;
            ++prev_el;
        }

        //check that expected and return timestamps agree
        BOOST_CHECK_MESSAGE(expected_el->get_timestamp()==return_el->get_timestamp(),
                            "Expected ts{" << expected_el->get_timestamp() << "} == return ts{" << return_el->get_timestamp() << "} for test_ts=" << test_ts);

        //check that we satisfy the lower bound condition
        BOOST_CHECK_MESSAGE(return_el->get_timestamp()>=test_ts,
                            "Returned ts{" << return_el->get_timestamp() << "} is >= test_ts{" << test_ts << "}");
        BOOST_CHECK_MESSAGE((prev_el->get_timestamp()<test_ts || return_el==buffer->begin()),
                            "Prev ts{" << prev_el->get_timestamp() << "} is < test_ts{" << test_ts << "} (or lower bound is begin of buffer)");
    }; //end test_lower_bounds



    /*
     * Unskipped buffer should have elements with index [0, 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 ]
     *                                   and timestamps [0,1*T,2*T,3*T,4*T,5*T,6*T,7*T,8*T,9*T]
     * where T = DTS ticks between successive elements (tick_diff_per_frame * n_frames_per_obj_in_buffer)
     */
    BOOST_TEST_MESSAGE("Testing buffer without skips...");
    auto buffer_noskip = std::make_shared< BufferType<TypeAdapter> >();
    fill_buffer<BufferType,TypeAdapter>(buffer_noskip,0,10);
    print_buffer<BufferType,TypeAdapter>(buffer_noskip,"noskip");

    // get lower bound on aligned element
    // should return the exact value
    test_lower_bound(buffer_noskip,ticks_between*2,2);

    // get lower bound when maximally unaligned
    // should get the next element up
    test_lower_bound(buffer_noskip,ticks_between*5/2,3);

    // get lower bound when minimally unaligned, next instance
    test_lower_bound(buffer_noskip,ticks_between+1,2);

    /*
     * Skipped buffer should have elements with index [0, 1 , 2 , 3 , 4 , 5 , 6 , 7 ,  8 ,  9 ]
     *                                 and timestamps [0,1*T,2*T,5*T,6*T,7*T,8*T,9*T,10*T,11*T]
     * where T = DTS ticks between successive elements (tick_diff_per_frame * n_frames_per_obj_in_buffer)
     */
    BOOST_TEST_MESSAGE("Testing buffer with skips...");
    std::set<size_t> obj_to_skip = {2,3};
    auto buffer_skip = std::make_shared< BufferType<TypeAdapter> >();
    fill_buffer<BufferType,TypeAdapter>(buffer_skip,0,10,obj_to_skip);
    print_buffer<BufferType,TypeAdapter>(buffer_skip,"skip");

    // get lower bound on aligned but skipped element
    // should return next available
    test_lower_bound(buffer_skip,ticks_between*2,2,true);
    // should be unaffected
    test_lower_bound(buffer_skip,ticks_between,1,true);

    // get lower bound when maximally unaligned
    // should return next available
    test_lower_bound(buffer_skip,ticks_between*3/2,2,true);
    test_lower_bound(buffer_skip,ticks_between*5/2,2,true);
    test_lower_bound(buffer_skip,ticks_between*7/2,2,true);
    // should be unaffected
    test_lower_bound(buffer_skip,ticks_between*1/2,1,true);
    test_lower_bound(buffer_skip,ticks_between*9/2,3,true);
    test_lower_bound(buffer_skip,ticks_between*11/2,4,true);

    // get lower bound when minimally unaligned, next instance
    test_lower_bound(buffer_skip,ticks_between+1,2,true);
    test_lower_bound(buffer_skip,ticks_between*2+1,2,true);
    // should be unaffected
    test_lower_bound(buffer_skip,1,1,true);

}//end test_queue_model


template<class ReadoutType, class LatencyBufferType>
class TestDefaultRequestHandlerModel : public dunedaq::datahandlinglibs::DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>
{
public:
    TestDefaultRequestHandlerModel(std::shared_ptr<LatencyBufferType>& latency_buffer,
                                   std::unique_ptr<dunedaq::datahandlinglibs::FrameErrorRegistry>& error_registry)
      : dunedaq::datahandlinglibs::DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>(latency_buffer,error_registry) {}
    using dunedaq::datahandlinglibs::DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>::get_fragment_pieces;
};

template <template<class> class BufferType, class TypeAdapter>
void test_request_model(){

    using DefaultRequestHandler = TestDefaultRequestHandlerModel<TypeAdapter, BufferType<TypeAdapter> >;

    //some testing vars
    TypeAdapter test_element;
    uint64_t n_frames = test_element.get_num_frames();
    uint64_t ticks_per_frame = TypeAdapter::expected_tick_difference;
    uint64_t ticks_between = ticks_per_frame*n_frames;


    // function for testing get_fragment_pieces in cases where there are no skips in the buffer
    auto test_req_bounds = [&](std::shared_ptr<BufferType<TypeAdapter>> buffer,
                               uint64_t start_win, uint64_t end_win,
                               std::set<size_t> objects_skipped={}){

        //make the error registry we need
        auto errorRegistry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();

        //create the request handler
        DefaultRequestHandler requestHandler(buffer,errorRegistry);

        //call the get_fragment_pieces
        auto dfmessage = dunedaq::dfmessages::DataRequest();
        auto req_res = typename DefaultRequestHandler::RequestResult(DefaultRequestHandler::ResultCode::kUnknown,dfmessage);
        auto ret = requestHandler.get_fragment_pieces(start_win,end_win,req_res);

        //check that the return code is correct.
        BOOST_CHECK_EQUAL(req_res.result_code,DefaultRequestHandler::ResultCode::kFound);

        //check that the first and last returned blocks are note empty
        BOOST_REQUIRE_GT(ret.front().second,0);
        BOOST_REQUIRE_GT(ret.back().second,0);

        //grab the (first) timestamps of the first and last frames
        uint64_t first_ts = reinterpret_cast<const TypeAdapter::FrameType*>(ret.front().first)->get_timestamp();
        uint64_t last_ts = reinterpret_cast<const TypeAdapter::FrameType*>((char*)(ret.back().first)+(ret.back().second)-sizeof(typename TypeAdapter::FrameType))->get_timestamp();

        //general check:
        // first_ts <= start_win < first_ts + ticks_per_frame
        // last_ts < end_win <= last_ts + ticks_per_frame
        if(objects_skipped.size()==0)
            BOOST_CHECK_MESSAGE(first_ts<=start_win,
                                "first_frame_ts{" << first_ts << "} <= start_win{" << start_win << "}");
        BOOST_CHECK_MESSAGE(start_win<first_ts+ticks_per_frame,
                            "start_win{" << start_win << "} < first_frame_ts+ticks_per_frame{" << first_ts+ticks_per_frame << "}");
        BOOST_CHECK_MESSAGE(last_ts<end_win,
                            "Check last_frame_ts{" << last_ts << "} < end_win{" << end_win << "}");
        if(objects_skipped.size()==0)
            BOOST_CHECK_MESSAGE(end_win<=last_ts+ticks_per_frame,
                                "end_win{" << end_win << "} <= last_frame_ts+ticks_per_frame{" << last_ts+ticks_per_frame << "}");

        //expected timestamps for begin of fragment and 'end' of fragment, assuming no skipping
        auto expected_start = TypeAdapter::expected_tick_difference *
                (uint64_t) std::floor((float) (start_win) / (float) (ticks_per_frame));
        auto expected_end = TypeAdapter::expected_tick_difference *
                (uint64_t) std::ceil((float) (end_win) / (float) (ticks_per_frame));

        //correct for the cases there that object has been skipped
        auto expected_start_obj = expected_start - expected_start%ticks_between;
        while(objects_skipped.count(expected_start_obj/ticks_between)>0) {
            expected_start += ticks_per_frame;
            expected_start_obj = expected_start - expected_start%ticks_between;
        }

        auto expected_end_obj = expected_end - ticks_per_frame - (expected_end-ticks_per_frame)%ticks_between;
        while(objects_skipped.count(expected_end_obj/ticks_between)>0) {
            expected_end -= ticks_per_frame;
            expected_start_obj = expected_end - ticks_per_frame - (expected_end-ticks_per_frame)%ticks_between;
        }

        //specfic check: are values for this request what we expect
        BOOST_CHECK_MESSAGE(first_ts == expected_start,
                            "Fragment start ts {" << first_ts << "} is expected value {" << expected_start << "}");
        BOOST_CHECK_MESSAGE((last_ts + TypeAdapter::expected_tick_difference) == expected_end,
                            "Fragment 'end' ts {" << last_ts + TypeAdapter::expected_tick_difference
                            << "} is expected value {" << expected_end << "}");
    };//end test_req_bounds

    /*
     * Unskipped buffer should have elements with index [0, 1 , 2 , 3 , 4 , 5 , 6 , 7 , 8 , 9 ]
     *                                   and timestamps [0,1*T,2*T,3*T,4*T,5*T,6*T,7*T,8*T,9*T]
     * where T = DTS ticks between successive elements (tick_diff_per_frame * n_frames_per_obj_in_buffer)
    */
    BOOST_TEST_MESSAGE("Testing buffer without skips...");
    auto buffer_noskip = std::make_shared< BufferType<TypeAdapter> >();
    fill_buffer<BufferType,TypeAdapter>(buffer_noskip,0,10);
    print_buffer<BufferType,TypeAdapter>(buffer_noskip,"noskip");

    test_req_bounds(buffer_noskip,ticks_between*2,ticks_between*5);
    test_req_bounds(buffer_noskip,ticks_between*3/2,ticks_between*9/2);
    test_req_bounds(buffer_noskip,ticks_between*11/5,ticks_between*21/5);
    test_req_bounds(buffer_noskip,ticks_between*2+1,ticks_between*5+1);

    /*
     * Skipped buffer should have elements with index [0, 1 , 2 , 3 , 4 , 5 , 6 , 7 ,  8 ,  9 ]
     *                                 and timestamps [0,1*T,2*T,5*T,6*T,7*T,8*T,9*T,10*T,11*T]
     * where T = DTS ticks between successive elements (tick_diff_per_frame * n_frames_per_obj_in_buffer)
     */
    BOOST_TEST_MESSAGE("Testing buffer with skips...");
    std::set<size_t> obj_to_skip = {2,3};
    auto buffer_skip = std::make_shared< BufferType<TypeAdapter> >();
    fill_buffer<BufferType,TypeAdapter>(buffer_skip,0,10,obj_to_skip);
    print_buffer<BufferType,TypeAdapter>(buffer_skip,"skip");

    test_req_bounds(buffer_skip,ticks_between*2,ticks_between*5,obj_to_skip);
    test_req_bounds(buffer_skip,ticks_between*3/2,ticks_between*9/2,obj_to_skip);
    test_req_bounds(buffer_skip,ticks_between*11/5,ticks_between*21/5,obj_to_skip);
    test_req_bounds(buffer_skip,ticks_between*2+1,ticks_between*5+1,obj_to_skip);

}//end test_request_model

}//namespace test
}//namespace datahandlinglibs
}//namespace dunedaq


#endif //DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_TESTUTILITIES_HPP
