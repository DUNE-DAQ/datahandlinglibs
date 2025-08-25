#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP

#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/models/TaskRawDataProcessorModel.hpp"
#include "datahandlinglibs/models/EmptyFragmentRequestHandlerModel.hpp"
//#include "daqdataformats/Fragment.hpp"  // for FragmentType

#include <deque>



namespace dunedaq{
namespace datahandlinglibs{
namespace unittest{

using namespace dunedaq::datahandlinglibs;

struct FakeReadoutType {
    uint64_t get_timestamp() const {return timestamp;}
    void set_timestamp(uint64_t ts) {timestamp = ts;}
    size_t get_payload_size() {return fixed_payload_size;}
    size_t get_frame_size() {return fixed_frame_size;}
    size_t get_num_frames() {return fixed_num_frames;}
    FakeReadoutType* begin() {return nullptr;}
    FakeReadoutType* end() {return nullptr;}

    bool operator<(const struct FakeReadoutType& other) const{return (this->timestamp < other.timestamp);}
    bool operator==(const struct FakeReadoutType& other) const {return timestamp == other.timestamp;}
    FakeReadoutType operator*(const FakeReadoutType& other) const {
        FakeReadoutType result;
        result.timestamp = this->timestamp * other.timestamp;
        return result;
    }
    FakeReadoutType operator+(int value) const {
        FakeReadoutType result;
        result.timestamp = this->timestamp + value;
        return result;
    }   

    uint64_t timestamp; // NOLINT(build/unsigned)
    static constexpr size_t fixed_frame_size = 1024; 
    static constexpr size_t fixed_num_frames = 100;

    static const constexpr dunedaq::daqdataformats::SourceID::Subsystem subsystem =
        dunedaq::daqdataformats::SourceID::Subsystem::kUnknown;
    static const constexpr dunedaq::daqdataformats::FragmentType fragment_type =
        dunedaq::daqdataformats::FragmentType::kUnknown;        
    static const constexpr uint64_t expected_tick_difference =
        0;
    static constexpr size_t fixed_payload_size = 100;

    
};
inline std::ostream& operator<<(std::ostream& os, const FakeReadoutType& obj) {
    os << "FakeReadoutType(timestamp=" << obj.timestamp << ")";
    return os;
}

struct FakeIterator {
    using value_type = FakeReadoutType;

    FakeIterator(FakeReadoutType* ptr = nullptr) : ptr_(ptr) {}

    FakeReadoutType& operator*() const { return *ptr_; }
    FakeReadoutType* operator->() const { return ptr_; }

    FakeIterator& operator++() {
        ++ptr_;
        return *this;
    }

    friend bool operator!=(const FakeIterator& a, const FakeIterator& b) {
        return a.ptr_ != b.ptr_;
    }

    friend bool operator==(const FakeIterator& a, const FakeIterator& b) {
        return a.ptr_ == b.ptr_;
    }

    bool good() const { return ptr_ != nullptr; }

private:
    FakeReadoutType* ptr_;
};

template<class T>
class FakeLatencyBufferType : public LatencyBufferConcept<T> {
public:
    void conf([[maybe_unused]] const dunedaq::appmodel::LatencyBuffer* conf) override {}
    void scrap([[maybe_unused]] const nlohmann::json& cfg) override {}
    std::size_t occupancy() const override {return buffer_.size();}
    void flush() override {buffer_.clear();}
    bool write([[maybe_unused]] T&& element) override { 
        buffer_.push_back(std::move(element)); 
        return true; 
    }
    const T* back() override {return buffer_.empty() ? nullptr : &buffer_.back();}
    const T* front() override {return buffer_.empty() ? nullptr : &buffer_.front();}

    bool read(T& element) override {
        if (buffer_.empty()) return false;
        element = buffer_.front();
        return true;
    }
    void pop(std::size_t amount) override {
        while (amount-- && !buffer_.empty()) {
            buffer_.pop_front();
        }
    }
    void allocate_memory([[maybe_unused]] size_t /*size*/) override {};

    FakeIterator lower_bound([[maybe_unused]] T& element,[[maybe_unused]] bool with_errors=false) {return begin();}
    FakeIterator end(){return buffer_.empty() ? FakeIterator{nullptr} : FakeIterator{&buffer_.back() + 1};}
    FakeIterator begin() {return buffer_.empty() ? FakeIterator{nullptr} : FakeIterator{&buffer_.front()};}


    size_t get_alignment_size() const {return alignof(T); }
    size_t size() const {return buffer_.size();}
    const T* start_of_buffer() const {return buffer_.empty() ? nullptr : &buffer_.front();}
    const T* end_of_buffer() const {return buffer_.empty() ? nullptr : &buffer_.back() + 1;}

    std::deque<T> buffer_;    
    
};

class FakeRequestHandlerType : public DefaultRequestHandlerModel<unittest::FakeReadoutType, unittest::FakeLatencyBufferType<unittest::FakeReadoutType>> {
public:
    FakeRequestHandlerType(std::shared_ptr<unittest::FakeLatencyBufferType<unittest::FakeReadoutType>>& latency_buffer,
        std::unique_ptr<FrameErrorRegistry>& error_registry) 
    : DefaultRequestHandlerModel<unittest::FakeReadoutType, unittest::FakeLatencyBufferType<unittest::FakeReadoutType>>(latency_buffer, error_registry)    
    {}
};

class FakeRawDataProcessorType : public dunedaq::datahandlinglibs::TaskRawDataProcessorModel<unittest::FakeReadoutType> {
public:
    FakeRawDataProcessorType(std::unique_ptr<unittest::FrameErrorRegistry>& error_registry, bool post_processing_enabled) 
    : TaskRawDataProcessorModel<unittest::FakeReadoutType>(error_registry, post_processing_enabled)
    {}
};


}
}
}

#endif //DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP