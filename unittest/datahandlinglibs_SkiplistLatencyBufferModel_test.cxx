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

#include "datahandlinglibs/opmon/datahandling_info.pb.h"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"

#include "datahandlinglibs/testutils/UnitTestUtilities.hpp"
#include "folly/ConcurrentSkipList.h"

using namespace dunedaq::datahandlinglibs;

BOOST_AUTO_TEST_SUITE(datahandlinglibs_SkiplistLatencyBufferModel_test)

/**
 * @brief Tests occupancy, write,put,read, flush funtions functions
 */
BOOST_AUTO_TEST_CASE(SkiplistLatencyBufferModel_put)
{
  SkipListLatencyBufferModel<unittest::MockReadoutType> skip_list;

  unittest::MockReadoutType dummy_put;
  int size = 5;

  for (int i = 0; i < size; i++) {
    dummy_put.set_timestamp(i * i);
    BOOST_REQUIRE(skip_list.put(dummy_put));
  }

  BOOST_REQUIRE_EQUAL(skip_list.occupancy(), size);
}

BOOST_AUTO_TEST_CASE(SkiplistLatencyBufferModel_write)
{
  SkipListLatencyBufferModel<unittest::MockReadoutType> skip_list;
  int size = 5;

  for (int i = 0; i < size; i++) {
    unittest::MockReadoutType dummy_move;
    dummy_move.set_timestamp(i * i);
    BOOST_REQUIRE(skip_list.write(std::move(dummy_move)));
  }

  BOOST_REQUIRE_EQUAL(skip_list.occupancy(), size);
}

BOOST_AUTO_TEST_CASE(SkiplistLatencyBufferModel_pop)
{
  SkipListLatencyBufferModel<unittest::MockReadoutType> skip_list;

  unittest::MockReadoutType dummy_put;
  for (int i = 0; i < 10; i++) {
    dummy_put.set_timestamp(i * i);
    skip_list.put(dummy_put);
  }

  skip_list.pop(3);

  BOOST_REQUIRE_EQUAL(skip_list.occupancy(), 7);
}

BOOST_AUTO_TEST_CASE(SkiplistLatencyBufferModel_read)
{
  SkipListLatencyBufferModel<unittest::MockReadoutType> skip_list;

  unittest::MockReadoutType dummy_put;
  for (int i = 0; i < 10; i++) {
    dummy_put.set_timestamp(i * i);
    skip_list.put(dummy_put);
  }

  unittest::MockReadoutType dummy_read;

  BOOST_REQUIRE(skip_list.read(dummy_read));
  BOOST_CHECK_EQUAL(dummy_read.get_timestamp(), 0);
  BOOST_REQUIRE_EQUAL(skip_list.occupancy(), 10);
}

BOOST_AUTO_TEST_CASE(SkiplistLatencyBufferModel_flush)
{
  SkipListLatencyBufferModel<unittest::MockReadoutType> skip_list;

  unittest::MockReadoutType dummy_put;
  for (int i = 0; i < 10; i++) {
    dummy_put.set_timestamp(i * i);
    skip_list.put(dummy_put);
  }

  skip_list.flush();

  BOOST_REQUIRE_EQUAL(skip_list.occupancy(), 0);
}

BOOST_AUTO_TEST_CASE(SkiplistLatencyBufferModel_lower_bound)
{
  SkipListLatencyBufferModel<unittest::MockReadoutType> skip_list;
  unittest::MockReadoutType dummy_put;

  for (int i = 0; i < 5; i++) {
    dummy_put.set_timestamp(i);
    skip_list.put(dummy_put);
  }

  unittest::MockReadoutType dummy_bound;
  dummy_bound.set_timestamp(4);
  auto it_lower = skip_list.lower_bound(dummy_bound);

  unittest::MockReadoutType& d = *it_lower;
  BOOST_REQUIRE_EQUAL(d.get_timestamp(), 4);
}
BOOST_AUTO_TEST_CASE(SkiplistLatencyBufferModel_iterators)
{
  SkipListLatencyBufferModel<unittest::MockReadoutType> skip_list;
  unittest::MockReadoutType dummy_put;

  for (int i = 0; i < 5; i++) {
    dummy_put.set_timestamp(i);
    skip_list.put(dummy_put);
  }

  auto it = skip_list.begin();
  auto it_end = skip_list.end();

  int val = 0;
  while (it != it_end) {
    BOOST_REQUIRE_EQUAL(it->get_timestamp(), val);
    BOOST_REQUIRE(it.good());
    ++it;
    ++val;
  }
  BOOST_REQUIRE(!it.good());
}

BOOST_AUTO_TEST_CASE(SkiplistLatencyBufferModel_front_and_back_pointers)
{
  SkipListLatencyBufferModel<unittest::MockReadoutType> skip_list;
  unittest::MockReadoutType dummy_put;

  for (int i = 0; i < 5; i++) {
    dummy_put.set_timestamp(i);
    skip_list.put(dummy_put);
  }

  const unittest::MockReadoutType* front_ptr = skip_list.front();
  const unittest::MockReadoutType* back_ptr = skip_list.back();

  BOOST_REQUIRE(front_ptr != nullptr);
  BOOST_REQUIRE(back_ptr != nullptr);

  BOOST_REQUIRE_EQUAL(front_ptr->get_timestamp(), 0);
  BOOST_REQUIRE_EQUAL(back_ptr->get_timestamp(), 4);
}

BOOST_AUTO_TEST_CASE(SkiplistLatencyBufferModel_get_skiplist)
{
  SkipListLatencyBufferModel<unittest::MockReadoutType> skip_list;
  unittest::MockReadoutType dummy_put;

  for (int i = 0; i < 5; i++) {
    dummy_put.set_timestamp(i);
    skip_list.put(dummy_put);
  }

  folly::ConcurrentSkipList<unittest::MockReadoutType>::Accessor acc(skip_list.get_skip_list());

  BOOST_REQUIRE_EQUAL(skip_list.front(), acc.first());
}

BOOST_AUTO_TEST_SUITE_END()
