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
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"
#include "datahandlinglibs/models/SourceEmulatorModel.hpp"
#include "datahandlinglibs/models/TaskRawDataProcessorModel.hpp"
#include "datahandlinglibs/models/ZeroCopyRecordingRequestHandlerModel.hpp"
#include "datahandlinglibs/ReadoutTypes.hpp"



#include "datahandlinglibs/opmon/datahandling_info.pb.h"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"

#include <cassert>



using namespace dunedaq::datahandlinglibs;
using namespace folly;

struct FakeReadoutType {
    uint64_t get_timestamp() const {return 0;}
    void set_timestamp([[maybe_unused]]uint64_t ts) {}
    size_t get_payload_size() {return 0;}
    size_t get_frame_size() {return 0;}
    size_t get_num_frames() {return 0;}
    FakeReadoutType* begin() {return nullptr;}
    FakeReadoutType* end() {return nullptr;}

    bool operator<([[maybe_unused]]const struct FakeReadoutType& other) const{return true;}

    static const constexpr dunedaq::daqdataformats::SourceID::Subsystem subsystem =
        dunedaq::daqdataformats::SourceID::Subsystem::kUnknown;
    static const constexpr dunedaq::daqdataformats::FragmentType fragment_type =
        dunedaq::daqdataformats::FragmentType::kUnknown;        
    static const constexpr uint64_t expected_tick_difference =
        0;
    static constexpr size_t fixed_payload_size = 100;
};

struct FakeIterator {
    mutable FakeReadoutType dummy;

    FakeReadoutType& operator*() const {return dummy;}
    FakeReadoutType* operator->() {return &dummy;}
    FakeIterator& operator++() {return *this;}
    friend bool operator!=([[maybe_unused]]const FakeIterator& a,[[maybe_unused]] const FakeIterator& b) {return true;}    
    bool good() {return true;}

    friend bool operator==([[maybe_unused]]const FakeIterator& lhs, [[maybe_unused]]const FakeIterator& rhs) {return true;}
};

template<class T>
class FakeLatencyBufferType : public LatencyBufferConcept<T> {
public:
    void conf([[maybe_unused]] const dunedaq::appmodel::LatencyBuffer* conf) override {}
    void scrap([[maybe_unused]] const nlohmann::json& cfg) override {}
    std::size_t occupancy() const override {return 0;}
    void flush() override {}
    bool write([[maybe_unused]] T&& element) override { return true;}
    const T* back() override {return nullptr;}
    const T* front() override {return nullptr;}

    bool read([[maybe_unused]] T& element) override {return true;};
    void pop([[maybe_unused]] std::size_t amount) override {};
    void allocate_memory([[maybe_unused]] size_t /*size*/) override {};

    mutable FakeIterator dummy;
    FakeIterator lower_bound([[maybe_unused]] T& element,[[maybe_unused]] bool with_errors=false) {return dummy;}
    FakeIterator end(){return dummy;}
    FakeIterator begin() const {return dummy;}


    size_t get_alignment_size() const {return 0;}
    size_t size() const {return 0;}
    const T* start_of_buffer() const {
        static T dummy_buffer[1024]; // Simple static buffer for mock
        return dummy_buffer;
    }
    const T* end_of_buffer() const {
        static T dummy_buffer[1024];
        return dummy_buffer + size();
    }

    
    
};

class FakeRequestHandlerType : public DefaultRequestHandlerModel<FakeReadoutType, FakeLatencyBufferType<FakeReadoutType>> {
public:
    FakeRequestHandlerType(std::shared_ptr<FakeLatencyBufferType<FakeReadoutType>>& latency_buffer,
        std::unique_ptr<FrameErrorRegistry>& error_registry) 
    : DefaultRequestHandlerModel<FakeReadoutType, FakeLatencyBufferType<FakeReadoutType>>(latency_buffer, error_registry)    
    {}
};

class FakeRawDataProcessorType : public TaskRawDataProcessorModel<FakeReadoutType> {
public:
    FakeRawDataProcessorType(std::unique_ptr<FrameErrorRegistry>& error_registry, bool post_processing_enabled) 
    : TaskRawDataProcessorModel<FakeReadoutType>(error_registry, post_processing_enabled)
    {}
};




int
main(int /*argc*/, char** /*argv[]*/)
{
    
    BinarySearchQueueModel <types::DUMMY_FRAME_STRUCT> a;
    std::atomic<bool> flag{true};
    /*
    DataHandlingModel<
    types::DUMMY_FRAME_STRUCT,         // Readout
    DefaultSkipListRequestHandler<types::DUMMY_FRAME_STRUCT>,      // Request Handler
    SkipListLatencyBufferModel<types::DUMMY_FRAME_STRUCT>,         // Latency Buffer
    DummyRawDataProcessor<types::DUMMY_FRAME_STRUCT>,              // Raw Processor 
    types::DUMMY_FRAME_STRUCT                                      // Input Type
    > s(flag);
    */

    std::atomic<bool> run_marker;

    auto readout_model = std::make_shared<DataHandlingModel<
    FakeReadoutType,
    FakeRequestHandlerType,
    FakeLatencyBufferType<FakeReadoutType>,
    FakeRawDataProcessorType,
    int>>(run_marker);

    DataSubscriberModel <types::DUMMY_FRAME_STRUCT> d;
    auto latency_buffer = std::make_shared<FakeLatencyBufferType<FakeReadoutType>>();
    auto error_registry = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();

    DefaultRequestHandlerModel<FakeReadoutType, FakeLatencyBufferType<FakeReadoutType>> model(latency_buffer, error_registry);

    auto latency_buffer2 = std::make_shared<SkipListLatencyBufferModel<FakeReadoutType>>();
    auto error_registry2 = std::make_unique<dunedaq::datahandlinglibs::FrameErrorRegistry>();
    DefaultSkipListRequestHandler <FakeReadoutType> ty(latency_buffer2,error_registry2);
    
    EmptyFragmentRequestHandlerModel <FakeReadoutType,FakeLatencyBufferType<FakeReadoutType>>uu(latency_buffer,error_registry);

    FixedRateQueueModel <int> q;
    IterableQueueModel<int> w;
    RecorderModel <types::DUMMY_FRAME_STRUCT>qw("name");
    SkipListLatencyBufferModel <types::DUMMY_FRAME_STRUCT> gfsh;
    SourceEmulatorPatternGenerator jghj;
    TaskRawDataProcessorModel <FakeReadoutType> adfg(error_registry,false);
    ZeroCopyRecordingRequestHandlerModel <FakeReadoutType,FakeLatencyBufferType<FakeReadoutType>> fgs(latency_buffer,error_registry);
    



    
    


  return 0;
} // NOLINT(readability/fn_size)
