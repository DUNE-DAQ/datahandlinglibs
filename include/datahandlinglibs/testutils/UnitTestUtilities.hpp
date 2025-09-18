#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP

#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/models/TaskRawDataProcessorModel.hpp"
#include "datahandlinglibs/models/EmptyFragmentRequestHandlerModel.hpp"
#include "datahandlinglibs/models/DataHandlingModel.hpp"

#include <deque>

namespace dunedaq {
namespace datahandlinglibs {
namespace unittest {


struct FakeReadoutTypeBase
{
  uint64_t get_timestamp() const { return timestamp; }
  void set_timestamp(uint64_t ts) { timestamp = ts; }
  size_t get_payload_size() const { return fixed_num_frames * fixed_frame_size; }
  size_t get_frame_size() const { return fixed_frame_size; }
  virtual size_t get_num_frames() const { return fixed_num_frames; }
  virtual FakeReadoutTypeBase* begin() = 0;
  virtual FakeReadoutTypeBase* end() = 0;

  uint64_t timestamp; // NOLINT(build/unsigned)
  static constexpr size_t fixed_frame_size = 16;
  static constexpr size_t fixed_num_frames = 1;

  static const constexpr dunedaq::daqdataformats::SourceID::Subsystem subsystem =
    dunedaq::daqdataformats::SourceID::Subsystem::kUnknown;
  static const constexpr dunedaq::daqdataformats::FragmentType fragment_type =
    dunedaq::daqdataformats::FragmentType::kUnknown;
  static const constexpr uint64_t expected_tick_difference = 1;
  static constexpr size_t fixed_payload_size = fixed_frame_size * fixed_num_frames;

  char data[fixed_payload_size];

  virtual ~FakeReadoutTypeBase() = default;
};

struct FakeReadoutType : public FakeReadoutTypeBase
{
  size_t get_num_frames() const override { return fixed_num_frames; }
  FakeReadoutType* begin() override { return reinterpret_cast<FakeReadoutType*>(data); }

  FakeReadoutType* end() override { return begin() + fixed_num_frames; }

  static constexpr size_t fixed_num_frames = 1;
  static constexpr size_t fixed_payload_size = fixed_frame_size * fixed_num_frames;
  char data[fixed_payload_size];

  FakeReadoutType operator*(const FakeReadoutType& other) const
  {
    FakeReadoutType result;
    result.timestamp = this->timestamp * other.timestamp;
    return result;
  }
  FakeReadoutType operator+(int value) const
  {
    FakeReadoutType result;
    result.timestamp = this->timestamp + value;
    return result;
  }
  bool operator<(const FakeReadoutType& rhs) const { return this->timestamp < rhs.get_timestamp(); }
  bool operator==(const FakeReadoutType& rhs) const { return this->timestamp == rhs.get_timestamp(); }

  FakeReadoutType() {}
};
inline std::ostream&
operator<<(std::ostream& os, const FakeReadoutType& obj)
{
  os << "FakeReadoutType(timestamp=" << obj.timestamp << ")";
  return os;
}

struct FakeSuperChunkReadoutType : public FakeReadoutTypeBase
{
  size_t get_num_frames() const override { return fixed_num_frames; }
  FakeSuperChunkReadoutType* begin() override { return reinterpret_cast<FakeSuperChunkReadoutType*>(data); }
  FakeSuperChunkReadoutType* end() override { return begin() + fixed_num_frames; }

  static constexpr size_t fixed_num_frames = 4;
  static constexpr size_t fixed_payload_size = fixed_frame_size * fixed_num_frames;
  char data[fixed_payload_size];

  FakeSuperChunkReadoutType operator*(const FakeSuperChunkReadoutType& other) const
  {
    FakeSuperChunkReadoutType result;
    result.timestamp = this->timestamp * other.timestamp;
    return result;
  }
  FakeSuperChunkReadoutType operator+(int value) const
  {
    FakeSuperChunkReadoutType result;
    result.timestamp = this->timestamp + value;
    return result;
  }
  bool operator<(const FakeSuperChunkReadoutType& rhs) const { return this->timestamp < rhs.get_timestamp(); }
  bool operator==(const FakeSuperChunkReadoutType& rhs) const { return this->timestamp == rhs.get_timestamp(); }

  FakeSuperChunkReadoutType() {}
};
inline std::ostream&
operator<<(std::ostream& os, const FakeSuperChunkReadoutType& obj)
{
  os << "FakeSuperChunkReadoutType(timestamp=" << obj.timestamp << ")";
  return os;
}

struct FakeIterator
{
  using value_type = FakeReadoutType;

  FakeIterator(FakeReadoutType* ptr = nullptr)
    : ptr_(ptr)
  {
  }

  FakeReadoutType& operator*() const { return *ptr_; }
  FakeReadoutType* operator->() const { return ptr_; }

  FakeIterator& operator++()
  {
    ++ptr_;
    return *this;
  }

  friend bool operator!=(const FakeIterator& a, const FakeIterator& b) { return a.ptr_ != b.ptr_; }

  friend bool operator==(const FakeIterator& a, const FakeIterator& b) { return a.ptr_ == b.ptr_; }

  bool good() const { return ptr_ != nullptr; }

private:
  FakeReadoutType* ptr_;
};

template<class T>
class FakeLatencyBufferType : public LatencyBufferConcept<T>
{
public:
  void conf([[maybe_unused]] const dunedaq::appmodel::LatencyBuffer* conf) override {}
  void scrap([[maybe_unused]] const appfwk::DAQModule::CommandData_t& cfg) override {}
  std::size_t occupancy() const override { return buffer_.size(); }
  void flush() override { buffer_.clear(); }
  bool write([[maybe_unused]] T&& element) override
  {
    buffer_.push_back(std::move(element));
    return true;
  }
  const T* back() override { return buffer_.empty() ? nullptr : &buffer_.back(); }
  const T* front() override { return buffer_.empty() ? nullptr : &buffer_.front(); }

  bool read(T& element) override
  {
    if (buffer_.empty())
      return false;
    element = buffer_.front();
    return true;
  }
  void pop(std::size_t amount) override
  {
    while (amount-- && !buffer_.empty()) {
      buffer_.pop_front();
    }
  }
  void allocate_memory([[maybe_unused]] size_t /*size*/) override{};

  FakeIterator lower_bound([[maybe_unused]] T& element, [[maybe_unused]] bool with_errors = false) { return begin(); }
  FakeIterator end() { return buffer_.empty() ? FakeIterator{ nullptr } : FakeIterator{ &buffer_.back() + 1 }; }
  FakeIterator begin() { return buffer_.empty() ? FakeIterator{ nullptr } : FakeIterator{ &buffer_.front() }; }

  size_t get_alignment_size() const { return alignof(T); }
  size_t size() const { return buffer_.size(); }
  const T* start_of_buffer() const { return buffer_.empty() ? nullptr : &buffer_.front(); }
  const T* end_of_buffer() const { return buffer_.empty() ? nullptr : &buffer_.back() + 1; }

  std::deque<T> buffer_;
};

template<class ReadoutType, class LatencyBufferType>
class FakeRequestHandlerType : public DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>
{
public:
  using DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>::DefaultRequestHandlerModel;

  int get_handled_requests() const { return (this->m_handled_requests).load(); }
  int get_response_time_acc() const { return (this->m_response_time_acc).load(); }
  int get_response_time_min() const { return (this->m_response_time_min).load(); }
  int get_response_time_max() const { return (this->m_response_time_max).load(); }
  int get_num_buffer_cleanups() const { return (this->m_num_buffer_cleanups).load(); }
  int get_waiting_requests() const { return (this->m_waiting_requests).size(); }
  int get_pops_count() const { return (this->m_pops_count).load(); }
  int get_occupancy() const { return (this->m_occupancy).load(); }
  void change_run_marker(bool change) { (this->m_run_marker) = change; }
  bool get_run_marker() { return (this->m_run_marker).load(); }
  void public_dump_to_buffer(const void* data,
                             std::size_t size,
                             void* buffer,
                             uint32_t buffer_pos, // NOLINT(build/unsigned)
                             const std::size_t& buffer_size)
  {
    this->dump_to_buffer(data, size, buffer, buffer_pos, buffer_size);
  }

  void public_check_waiting_requests() { this->check_waiting_requests(); }
  std::unique_ptr<dunedaq::daqdataformats::Fragment> public_create_empty_fragment(
    const dunedaq::dfmessages::DataRequest& dr)
  {
    return this->create_empty_fragment(dr);
  }

  void public_periodic_cleanups() { this->periodic_cleanups(); }
  void test_start()
  {
    this->m_request_handler_thread_pool = std::make_unique<boost::asio::thread_pool>(1);
    this->m_fragment_send_timeout_ms = 100;

    this->m_pop_limit_size = 5;
    this->m_pop_size_pct = 0.5;
    reset_opmon_variables();
  }

  dunedaq::daqdataformats::timestamp_t cutoff;
  dunedaq::daqdataformats::timestamp_t get_cutoff_timestamp() override { return cutoff; }
  void set_cutoff_timestamp(dunedaq::daqdataformats::timestamp_t ncutoff) { cutoff = ncutoff; }
  void reset_opmon_variables()
  {
    this->m_num_requests_found = 0;
    this->m_num_requests_bad = 0;
    this->m_num_requests_old_window = 0;
    this->m_num_requests_delayed = 0;
    this->m_num_requests_uncategorized = 0;
    this->m_num_buffer_cleanups = 0;
    this->m_num_requests_timed_out = 0;
    this->m_handled_requests = 0;
    this->m_response_time_acc = 0;
    this->m_pop_reqs = 0;
    this->m_pops_count = 0;
    this->m_payloads_written = 0;
    this->m_bytes_written = 0;
  }

  int get_num_requests_found() { return this->m_num_requests_found.load(); }
  int get_num_requests_delayed() { return this->m_num_requests_delayed.load(); }
  int get_num_requests_old_window() { return this->m_num_requests_old_window.load(); }
  int get_num_requests_bad() { return this->m_num_requests_bad.load(); }
};

class FakeRawDataProcessorType : public dunedaq::datahandlinglibs::TaskRawDataProcessorModel<unittest::FakeReadoutType>
{
public:
  using Base = dunedaq::datahandlinglibs::TaskRawDataProcessorModel<unittest::FakeReadoutType>;
  using Base::Base;

  void public_post_processing_thread(std::function<void(const unittest::FakeReadoutType*)>& func,
                                     folly::ProducerConsumerQueue<const unittest::FakeReadoutType*>& queue)
  {
    this->run_post_processing_thread(func, queue);
  }

  void public_post_processing_threads()
  {
    for (size_t i = 0; i < this->m_post_process_threads.size(); ++i) {
      this->m_post_process_threads[i]->set_work(&FakeRawDataProcessorType::public_post_processing_thread,
                                                this,
                                                std::ref(this->m_post_process_functions[i]),
                                                std::ref(*this->m_items_to_postprocess_queues[i]));
    }
  }

  void make_queues(int size)
  {
    for (size_t i = 0; i < this->m_post_process_functions.size(); ++i) {
      this->m_items_to_postprocess_queues.push_back(
        std::make_unique<folly::ProducerConsumerQueue<const unittest::FakeReadoutType*>>(size)); // size can be anything
      this->m_post_process_threads[i]->set_name(std::to_string(i), i);
    }
  }

  int get_post_queues_size() { return (this->m_items_to_postprocess_queues).size(); }

  const std::vector<std::unique_ptr<dunedaq::utilities::ReusableThread>>& get_post_threads() const
  {
    return this->m_post_process_threads;
  }
  std::vector<std::function<void(const unittest::FakeReadoutType*)>>& get_post_functions()
  {
    return this->m_post_process_functions;
  }
  const std::vector<std::unique_ptr<folly::ProducerConsumerQueue<const unittest::FakeReadoutType*>>>& get_post_queues()
    const
  {
    return this->m_items_to_postprocess_queues;
  }
  void run_marker_set(bool m_set) { this->m_run_marker = m_set; }
};

template<typename ReadoutType,
         typename RequestHandlerType,
         typename LatencyBufferType,
         typename RawDataProcessorType,
         typename InputDataType = ReadoutType>
class FakeDataHandlingModel
  : public datahandlinglibs::DataHandlingModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType, InputDataType>

{
  using Base = DataHandlingModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType, InputDataType>;

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

  void raw_processor(bool& pre_func_one_called, bool& post_func_one_called, int cutoff_timestamp)
  {
    this->m_raw_processor_impl->add_preprocess_task(
      [&pre_func_one_called](ReadoutType* /*elem*/) { pre_func_one_called = true; });
    this->m_raw_processor_impl->add_postprocess_task(
      [&post_func_one_called](const ReadoutType* /*elem*/) { post_func_one_called = true; });
    this->m_request_handler_supports_cutoff_timestamp = true;
    this->m_processing_delay_ticks = 0;
    this->m_request_handler_impl->set_cutoff_timestamp(cutoff_timestamp);
    this->m_raw_processor_impl->make_queues(32);
  }

  void post_schedule_init(bool& pre_func_one_called, bool& post_func_one_called, int cutoff_timestamp, int min_wait)
  {
    raw_processor(pre_func_one_called, post_func_one_called, cutoff_timestamp);

    this->m_postprocess_scheduler_thread.set_name("pprocsched", 1);
    this->m_timekeeper = std::make_unique<folly::ThreadWheelTimekeeper>();

    this->m_processing_delay_ticks = 4;
    this->m_post_processing_delay_max_wait = 100;
    this->m_num_post_processing_delay_max_waits = 0;
    this->m_post_processing_delay_min_wait = min_wait;
  }

  void public_process_item(ReadoutType&& payload) { this->process_item(std::move(payload)); }

  template<class IDT>
  std::vector<ReadoutType> public_transform_payload(IDT& payload)
  {
    return this->transform_payload(payload);
  }

  template<class IDT>
  void public_transform_and_process(IDT&& payload)
  {
    this->transform_and_process(std::forward<IDT>(payload));
  }
  template<class IDT>
  void public_consume_callback(IDT&& payload)
  {
    this->consume_callback(std::move(payload));
  }
  void public_run_postprocess_scheduler() { this->run_postprocess_scheduler(); }

  int get_m_num_payloads() { return (this->m_num_payloads).load(); }
  int get_m_sum_payloads() { return (this->m_sum_payloads).load(); }
  int get_m_stats_packet_count() { return (this->m_stats_packet_count).load(); }
  int get_raw_processor_queue_size() { return this->m_raw_processor_impl->get_post_queues_size(); }
  int get_m_num_post_processing_delay_max_waits() { return this->m_num_post_processing_delay_max_waits.load(); }
  int get_m_num_lb_insert_failures() { return static_cast<int>(this->m_num_lb_insert_failures.load()); }

  void change_m_run_marker(bool change) { this->m_run_marker = change; }
};

}
}
}

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP