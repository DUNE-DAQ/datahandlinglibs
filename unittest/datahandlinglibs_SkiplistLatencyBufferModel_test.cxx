/**
 * @file datahandlinglibs_SkiplistLatencyBufferModel_test.cxx Unit Tests for SkiplistLatencyBufferModel
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

/**
 * @brief Name of this test module
 */
#define BOOST_TEST_MODULE datahandlinglibs_SkiplistLatencyBufferModel_test // NOLINT

#include "boost/test/unit_test.hpp"

#include "datahandlinglibs/ReadoutTypes.hpp"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"

BOOST_AUTO_TEST_SUITE(datahandlinglibs_SkiplistLatencyBufferModel_test)

using namespace dunedaq::datahandlinglibs;

using ReadoutType = types::DUMMY_FRAME_STRUCT;

BOOST_AUTO_TEST_CASE(datahandlinglibs_SkiplistLatencyBufferModel_write)
{
  SkipListLatencyBufferModel<ReadoutType> buffer;

  ReadoutType frame1;
  frame1.timestamp = 2;
  frame1.another_key = 1;
  const auto [written1, result1] = buffer.write(std::move(frame1));
  BOOST_REQUIRE_EQUAL(result1, true);
  BOOST_REQUIRE_EQUAL(written1->get_timestamp(), 2);

  // Last written != back() in skip list since it is ordered
  ReadoutType frame2;
  frame2.timestamp = 1;
  frame2.another_key = 1;
  const auto [written2, result2] = buffer.write(std::move(frame2));
  BOOST_REQUIRE_EQUAL(result2, true);
  BOOST_REQUIRE_EQUAL(written2->get_timestamp(), 1);
  BOOST_REQUIRE_NE(written2, buffer.back());
  BOOST_REQUIRE_EQUAL(buffer.back()->get_timestamp(), 2);

  // Skip list doesn't accept duplicates, it returns an iterator to the existing element
  ReadoutType frame3;
  frame3.timestamp = 1;  
  frame3.another_key = 1;
  const auto [written3, result3] = buffer.write(std::move(frame3));
  BOOST_REQUIRE_EQUAL(result3, false);
  BOOST_REQUIRE_EQUAL(written3, written2);
}

BOOST_AUTO_TEST_SUITE_END()
