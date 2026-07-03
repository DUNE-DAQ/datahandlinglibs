/**
 * @file datahandlinglibs_IterableQueueModel_test.cxx Unit Tests for IterableQueueModel
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

/**
 * @brief Name of this test module
 */
#define BOOST_TEST_MODULE datahandlinglibs_IterableQueueModel_test // NOLINT

#include "boost/test/unit_test.hpp"

#include "datahandlinglibs/ReadoutTypes.hpp"
#include "datahandlinglibs/models/IterableQueueModel.hpp"

#include <utility>

BOOST_AUTO_TEST_SUITE(datahandlinglibs_IterableQueueModel_test)

using namespace dunedaq::datahandlinglibs;

using ReadoutType = types::DUMMY_FRAME_STRUCT;

BOOST_AUTO_TEST_CASE(datahandlinglibs_IterableQueueModel_write)
{
  const std::size_t size = 4;
  IterableQueueModel<ReadoutType> buffer(size);

  ReadoutType frame1;
  frame1.timestamp = 2;
  BOOST_REQUIRE_EQUAL(buffer.write(std::move(frame1)), true);
  BOOST_REQUIRE_EQUAL(buffer.back()->get_timestamp(), 2);  

  // Last written == back() in queue
  ReadoutType frame2;
  frame2.timestamp = 1;
  BOOST_REQUIRE_EQUAL(buffer.write(std::move(frame2)), true);
  BOOST_REQUIRE_EQUAL(buffer.back()->get_timestamp(), 1);

  // Queue accepts duplicates
  ReadoutType frame3;
  frame3.timestamp = 1;  
  BOOST_REQUIRE_EQUAL(buffer.write(std::move(frame3)), true);
  BOOST_REQUIRE_EQUAL(buffer.back()->get_timestamp(), 1);

  // Overflow (the actual capacity is size - 1, in this case it is 3)
  ReadoutType frame4;
  frame4.timestamp = 1;
  BOOST_REQUIRE_EQUAL(buffer.write(std::move(frame4)), false);
}

BOOST_AUTO_TEST_SUITE_END()
