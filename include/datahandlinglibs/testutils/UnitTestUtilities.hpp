/**
 * @file UnitTestUtilities.hpp Unit test helper classes
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP

#include "datahandlinglibs/models/DataHandlingModel.hpp"
#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/models/TaskRawDataProcessorModel.hpp"

namespace dunedaq {
namespace datahandlinglibs {
namespace unittest {


struct MockReadoutTypeBase
{
  uint64_t get_timestamp() const { return timestamp; }
  void set_timestamp(uint64_t ts) { timestamp = ts; }
  size_t get_payload_size() const { return fixed_num_frames * fixed_frame_size; }
  size_t get_frame_size() const { return fixed_frame_size; }
  virtual size_t get_num_frames() const { return fixed_num_frames; }
  virtual MockReadoutTypeBase* begin() = 0;
  virtual MockReadoutTypeBase* end() = 0;

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

  virtual ~MockReadoutTypeBase() = default;
};

struct MockReadoutType : public MockReadoutTypeBase
{
  size_t get_num_frames() const override { return fixed_num_frames; }
  MockReadoutType* begin() override { return reinterpret_cast<MockReadoutType*>(data); }

  MockReadoutType* end() override { return begin() + fixed_num_frames; }

  static constexpr size_t fixed_num_frames = 1;
  static constexpr size_t fixed_payload_size = fixed_frame_size * fixed_num_frames;
  char data[fixed_payload_size];

  MockReadoutType operator*(const MockReadoutType& other) const
  {
    MockReadoutType result;
    result.timestamp = this->timestamp * other.timestamp;
    return result;
  }
  MockReadoutType operator+(int value) const
  {
    MockReadoutType result;
    result.timestamp = this->timestamp + value;
    return result;
  }
  bool operator<(const MockReadoutType& rhs) const { return this->timestamp < rhs.get_timestamp(); }
  bool operator==(const MockReadoutType& rhs) const { return this->timestamp == rhs.get_timestamp(); }

  MockReadoutType() {}
};
inline std::ostream&
operator<<(std::ostream& os, const MockReadoutType& obj)
{
  os << "MockReadoutType(timestamp=" << obj.timestamp << ")";
  return os;
}

struct MockSuperChunkReadoutType : public MockReadoutTypeBase
{
  size_t get_num_frames() const override { return fixed_num_frames; }
  MockSuperChunkReadoutType* begin() override { return reinterpret_cast<MockSuperChunkReadoutType*>(data); }
  MockSuperChunkReadoutType* end() override { return begin() + fixed_num_frames; }

  static constexpr size_t fixed_num_frames = 4;
  static constexpr size_t fixed_payload_size = fixed_frame_size * fixed_num_frames;
  char data[fixed_payload_size];

  MockSuperChunkReadoutType operator*(const MockSuperChunkReadoutType& other) const
  {
    MockSuperChunkReadoutType result;
    result.timestamp = this->timestamp * other.timestamp;
    return result;
  }
  MockSuperChunkReadoutType operator+(int value) const
  {
    MockSuperChunkReadoutType result;
    result.timestamp = this->timestamp + value;
    return result;
  }
  bool operator<(const MockSuperChunkReadoutType& rhs) const { return this->timestamp < rhs.get_timestamp(); }
  bool operator==(const MockSuperChunkReadoutType& rhs) const { return this->timestamp == rhs.get_timestamp(); }

  MockSuperChunkReadoutType() {}
};
inline std::ostream&
operator<<(std::ostream& os, const MockSuperChunkReadoutType& obj)
{
  os << "MockSuperChunkReadoutType(timestamp=" << obj.timestamp << ")";
  return os;
}

struct MockIterator
{
  using value_type = MockReadoutType;

  MockIterator(MockReadoutType* ptr = nullptr)
    : ptr_(ptr)
  {
  }

  MockReadoutType& operator*() const { return *ptr_; }
  MockReadoutType* operator->() const { return ptr_; }

  MockIterator& operator++()
  {
    ++ptr_;
    return *this;
  }

  friend bool operator!=(const MockIterator& a, const MockIterator& b) { return a.ptr_ != b.ptr_; }

  friend bool operator==(const MockIterator& a, const MockIterator& b) { return a.ptr_ == b.ptr_; }

  bool good() const { return ptr_ != nullptr; }

private:
  MockReadoutType* ptr_;
};

template<class T>
class MockLatencyBufferType : public LatencyBufferConcept<T>
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

  MockIterator lower_bound([[maybe_unused]] T& element, [[maybe_unused]] bool with_errors = false) { return begin(); }
  MockIterator end() { return buffer_.empty() ? MockIterator{ nullptr } : MockIterator{ &buffer_.back() + 1 }; }
  MockIterator begin() { return buffer_.empty() ? MockIterator{ nullptr } : MockIterator{ &buffer_.front() }; }

  size_t get_alignment_size() const { return alignof(T); }
  size_t size() const { return buffer_.size(); }
  const T* start_of_buffer() const { return buffer_.empty() ? nullptr : &buffer_.front(); }
  const T* end_of_buffer() const { return buffer_.empty() ? nullptr : &buffer_.back() + 1; }

  std::deque<T> buffer_;
};

template<class ReadoutType, class LatencyBufferType>
class MockRequestHandlerType : public DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>
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
  void set_run_marker(bool change) { (this->m_run_marker) = change; }
  bool get_run_marker() { return (this->m_run_marker).load(); }
  void test_dump_to_buffer(const void* data,
                             std::size_t size,
                             void* buffer,
                             uint32_t buffer_pos, // NOLINT(build/unsigned)
                             const std::size_t& buffer_size)
  {
    this->dump_to_buffer(data, size, buffer, buffer_pos, buffer_size);
  }

  void test_check_waiting_requests() { this->check_waiting_requests(); }
  std::unique_ptr<dunedaq::daqdataformats::Fragment> test_create_empty_fragment(
    const dunedaq::dfmessages::DataRequest& dr)
  {
    return this->create_empty_fragment(dr);
  }

  void test_periodic_cleanups() { this->periodic_cleanups(); }
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

class MockRawDataProcessorType : public dunedaq::datahandlinglibs::TaskRawDataProcessorModel<unittest::MockReadoutType>
{
public:
  using Base = dunedaq::datahandlinglibs::TaskRawDataProcessorModel<unittest::MockReadoutType>;
  using Base::Base;

  void test_post_processing_thread(std::function<void(const unittest::MockReadoutType*)>& func,
                                     folly::ProducerConsumerQueue<const unittest::MockReadoutType*>& queue)
  {
    this->run_post_processing_thread(func, queue);
  }

  void test_post_processing_threads()
  {
    for (size_t i = 0; i < this->m_post_process_threads.size(); ++i) {
      this->m_post_process_threads[i]->set_work(&MockRawDataProcessorType::test_post_processing_thread,
                                                this,
                                                std::ref(this->m_post_process_functions[i]),
                                                std::ref(*this->m_items_to_postprocess_queues[i]));
    }
  }

  void make_queues(int size)
  {
    for (size_t i = 0; i < this->m_post_process_functions.size(); ++i) {
      this->m_items_to_postprocess_queues.push_back(
        std::make_unique<folly::ProducerConsumerQueue<const unittest::MockReadoutType*>>(size)); // size can be anything
      this->m_post_process_threads[i]->set_name(std::to_string(i), i);
    }
  }

  int get_post_queues_size() { return (this->m_items_to_postprocess_queues).size(); }

  const std::vector<std::unique_ptr<dunedaq::utilities::ReusableThread>>& get_post_threads() const
  {
    return this->m_post_process_threads;
  }
  std::vector<std::function<void(const unittest::MockReadoutType*)>>& get_post_functions()
  {
    return this->m_post_process_functions;
  }
  const std::vector<std::unique_ptr<folly::ProducerConsumerQueue<const unittest::MockReadoutType*>>>& get_post_queues()
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
class MockDataHandlingModel
  : public datahandlinglibs::DataHandlingModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType, InputDataType>

{
  using Base = DataHandlingModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType, InputDataType>;

public:
  using Base::Base; // Inherit all constructors from the base class
  using Base::PostprocessScheduleAlgorithm;
  using typename Base::num_post_processing_delay_max_waits_t;

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

  void test_process_item(ReadoutType&& payload) { this->process_item(std::move(payload)); }

  template<class IDT>
  std::vector<ReadoutType> test_transform_payload(IDT& payload)
  {
    return this->transform_payload(payload);
  }

  template<class IDT>
  void test_transform_and_process(IDT&& payload)
  {
    this->transform_and_process(std::forward<IDT>(payload));
  }
  template<class IDT>
  void test_consume_callback(IDT&& payload)
  {
    this->consume_callback(std::move(payload));
  }
  void test_run_postprocess_scheduler() { this->run_postprocess_scheduler(); }

  void test_run_postprocess_scheduler(
    std::shared_ptr<LatencyBufferType> latency_buffer_impl, std::shared_ptr<RawDataProcessorType> raw_processor_impl,
    std::unique_ptr<folly::Timekeeper> timekeeper, uint64_t post_processing_delay_max_wait)
  {
    this->m_latency_buffer_impl = latency_buffer_impl;
    this->m_raw_processor_impl = raw_processor_impl;
    this->m_timekeeper = std::move(timekeeper);
    this->m_post_processing_delay_max_wait = post_processing_delay_max_wait;
    this->run_postprocess_scheduler();
  }

  int get_num_payloads() { return (this->m_num_payloads).load(); }
  int get_sum_payloads() { return (this->m_sum_payloads).load(); }
  int get_stats_packet_count() { return (this->m_stats_packet_count).load(); }
  int get_raw_processor_queue_size() { return this->m_raw_processor_impl->get_post_queues_size(); }
  num_post_processing_delay_max_waits_t get_num_post_processing_delay_max_waits() { return this->m_num_post_processing_delay_max_waits.load(); }
  int get_num_lb_insert_failures() { return static_cast<int>(this->m_num_lb_insert_failures.load()); }

  void set_run_marker(bool change) { this->m_run_marker = change; }
};

} // namespace unittest
} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP
