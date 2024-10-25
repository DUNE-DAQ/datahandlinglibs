/**
 * @file SkipListLatencyBufferModel.hpp A folly concurrent SkipList wrapper
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_SKIPLISTLATENCYBUFFERMODEL_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_SKIPLISTLATENCYBUFFERMODEL_HPP_

#include "datahandlinglibs/DataHandlingIssues.hpp"
#include "datahandlinglibs/ReadoutLogging.hpp"
#include "datahandlinglibs/concepts/LatencyBufferConcept.hpp"

#include "logging/Logging.hpp"

#include "datahandlinglibs/opmon/datahandling_info.pb.h"

#include <folly/memory/ThreadCachedArena.h>
#include "folly/ConcurrentSkipList.h"

#include <memory>
#include <utility>

using dunedaq::datahandlinglibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace datahandlinglibs {

template<class T>
class SkipListLatencyBufferModel : public LatencyBufferConcept<T>
{

public:
  // Using shorter Folly typenames
  using ThreadCachedAlloc = folly::ThreadCachedArena;
  using AllocatorType = folly::CxxAllocatorAdaptor<T, ThreadCachedAlloc>;
  using SkipListT = folly::ConcurrentSkipList<T, std::less<T>, AllocatorType>;
  using SkipListTIter = typename SkipListT::iterator;
  using SkipListTAcc = typename SkipListT::Accessor; // SKL Accessor
  using SkipListTSkip = typename SkipListT::Skipper; // Skipper accessor

  // Constructor
  SkipListLatencyBufferModel()
    : m_arena(std::make_shared<ThreadCachedAlloc>())
    , m_allocator(m_arena)
    , m_skip_list(folly::ConcurrentSkipList<T>::createInstance(unconfigured_head_height, m_allocator))
  {
    m_arena->allocate(10000);
    TLOG(TLVL_WORK_STEPS) << "Initializing non configured latency buffer";
  }

  // Iterator for SkipList
  struct Iterator
  {
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;

    Iterator(SkipListTAcc&& acc, SkipListTIter iter)
      : m_acc(std::move(acc))
      , m_iter(iter)
    {}

    reference operator*() const { return *m_iter; }
    pointer operator->() { return &(*m_iter); }
    Iterator& operator++() // NOLINT(runtime/increment_decrement) :)
    {
      m_iter++;
      return *this;
    }

    friend bool operator==(const Iterator& a, const Iterator& b) { return a.m_iter == b.m_iter; }
    friend bool operator!=(const Iterator& a, const Iterator& b) { return a.m_iter != b.m_iter; }

    bool good() { return m_iter.good(); }


private:
    SkipListTAcc m_acc;
    SkipListTIter m_iter;
  };

  // Configure
  void conf(const appmodel::LatencyBuffer* /*conf*/) override
  {
    // Reset datastructure
    m_skip_list = folly::ConcurrentSkipList<T>::createInstance(unconfigured_head_height, m_allocator);
    m_arena->allocate(10000);
  }

  // Unconfigure
  void scrap(const nlohmann::json& /*args*/) override
  {
    // RS -> Cross-check, we don't need to flush first?
    m_skip_list = folly::ConcurrentSkipList<T>::createInstance(unconfigured_head_height);
  }

  // Get whole skip-list helper function
  std::shared_ptr<SkipListT>& get_skip_list() { return std::ref(m_skip_list); }

  // Override interface implementations
  size_t occupancy() const override;
  void flush() override { pop(occupancy()); }
  bool write(T&& new_element) override;
  bool put(T& new_element); // override
  bool read(T& element) override;

  // Iterator support
  Iterator begin();
  Iterator end();
  Iterator lower_bound(T& element, bool /*with_errors=false*/);

  // Front/back accessors override
  const T* front() override;
  const T* back() override;

  // Pop X override
  void pop(size_t num = 1) override; // NOLINT(build/unsigned)
protected:
    virtual void generate_opmon_data() override;  

private:
  // Arena
  std::shared_ptr<ThreadCachedAlloc> m_arena;

  // Allocator 
  AllocatorType m_allocator;

  // Concurrent SkipList
  std::shared_ptr<SkipListT> m_skip_list;

  // Configuration for datastructure head-hight
  static constexpr uint32_t unconfigured_head_height = 2; // NOLINT(build/unsigned)
};

} // namespace datahandlinglibs
} // namespace dunedaq

// Declarations
#include "detail/SkipListLatencyBufferModel.hxx"

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_SKIPLISTLATENCYBUFFERMODEL_HPP_
