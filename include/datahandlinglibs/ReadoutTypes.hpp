/**
 * @file ReadoutTypes.hpp Common types in datahandlinglibs
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_READOUTTYPES_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_READOUTTYPES_HPP_

#include <cstdint> // uint_t types
#include <memory>  // unique_ptr
#include <tuple>   // std::tie

#include <iostream>

namespace dunedaq {
namespace datahandlinglibs {
namespace types {

// Check for specific member fields
template<typename T>
concept HasTimestamp = requires(T t) {
  { t.timestamp } -> std::same_as<std::uint64_t&>;
};

template<typename T>
concept HasData = requires(T t) {
  std::is_same_v<decltype(std::declval<T>().timestamp), char[]>;
};

// Check for member functions
template<typename T>
concept HasGetTimestamp = requires(T t) {
  { t.get_timestamp() } -> std::same_as<std::uint64_t>;
};

template<typename T>
concept HasSetTimestamp = requires(T t, std::uint64_t ts) {
  t.set_timestamp(ts);
};

template<typename T>
concept HasOperatorLess = requires(T t, const T& other) {
  { t.operator<(other) } -> std::same_as<bool>;
};

// Check for static constexpr members
template<typename T>
concept HasFrameSize = requires {
    { T::frame_size } -> std::same_as<const size_t&>;
};

template<typename T>
concept HasFramesPerElement = requires {
  { T::frames_per_element } -> std::same_as<const std::uint8_t&>;
};

template<typename T>
concept HasElementSize = requires {
  { T::element_size } -> std::same_as<const size_t&>;
};

// Check begin() and end()
template<typename T>
concept HasBeginEnd = requires(T t) {
  { t.begin() } -> std::same_as<typename T::FrameType*>;
  { t.end() } -> std::same_as<typename T::FrameType*>;
};

// Combined concept to check all requirements for Data Handling Type
template<typename T>
concept IsDataHandlingCompliantType =
  HasTimestamp<T> &&
  HasData<T> &&
  HasGetTimestamp<T> &&
  HasSetTimestamp<T> &&
  HasOperatorLess<T> &&
  HasFrameSize<T> &&
  HasFramesPerElement<T> &&
  HasElementSize<T> &&
  HasBeginEnd<T>;

// Test compliance
template<typename T>
void checkDataHandlingCompliantType() {
    static_assert(IsDataHandlingCompliantType<T>, "Type does not meet the required interface for a Data Handling Type!");
}

struct ValidDataHandlingStruct {
    using FrameType = ValidDataHandlingStruct;

    uint64_t timestamp;
    uint64_t another_key;
    char data[1024];

    bool operator<(const ValidDataHandlingStruct& other) const {
        return std::tie(this->timestamp, this->another_key) < std::tie(other.timestamp, other.another_key);
    }

    uint64_t get_timestamp() const { return timestamp; }
    void set_timestamp(uint64_t ts) { timestamp = ts; }
    void set_another_key(uint64_t key) { another_key = key; }

    FrameType* begin() { return this; }
    FrameType* end() { return this + 1; }

    static const constexpr size_t frame_size = 1024;
    static const constexpr uint8_t frames_per_element = 1;
    static const constexpr size_t element_size = 1024;
};

const constexpr std::size_t DUMMY_FRAME_SIZE = 1024;
struct DUMMY_FRAME_STRUCT
{
  using FrameType = DUMMY_FRAME_STRUCT;

  // header
  uint64_t timestamp; // NOLINT(build/unsigned)
  uint64_t another_key; // NOLINT(build/unsigned)

  // data
  char data[DUMMY_FRAME_SIZE];

  // comparable based on composite key (timestamp + other unique keys)
  bool operator<(const DUMMY_FRAME_STRUCT& other) const
  {
    auto const & f1 = std::tie(this->timestamp, this->another_key);
    auto const & f2 = std::tie(other.timestamp, other.another_key);
    return f1 < f2;
  }

  uint64_t get_timestamp() const // NOLINT(build/unsigned)
  {
    return timestamp;
  }

  void set_another_key(uint64_t compkey)
  {
    another_key = compkey; 
  }

  void set_timestamp(uint64_t ts) // NOLINT(build/unsigned)
  {
    timestamp = ts;
  }

  void fake_timestamp(uint64_t /*first_timestamp*/, uint64_t /*offset = 25*/) // NOLINT(build/unsigned)
  {
    // tp.time_start = first_timestamp;
  }

  FrameType* begin() { return this; }

  FrameType* end() { return (this + 1); } // NOLINT

  static const constexpr size_t frame_size = DUMMY_FRAME_SIZE;
  static const constexpr uint8_t frames_per_element = 1; // NOLINT(build/unsigned)
  static const constexpr size_t element_size = DUMMY_FRAME_SIZE;
};

} // namespace types
} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_READOUTTYPES_HPP_
