/**
 * @file datahandlinglibs_ReadoutTypes_test.cxx Unit Tests for ReadoutTypes
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

/**
 * @brief Name of this test module
 */
#define BOOST_TEST_MODULE datahandlinglibs_ReadoutTypes_test // NOLINT

#include "boost/test/unit_test.hpp"

#include "datahandlinglibs/ReadoutTypes.hpp"

#include <iostream>
#include <string>

using namespace dunedaq::datahandlinglibs;

BOOST_AUTO_TEST_SUITE(datahandlinglibs_ReadoutTypes_test)

/**
 * @brief Tests setting and retrieving maximum timestamp and key values.
 */
BOOST_AUTO_TEST_CASE(ReadoutTypes_get_set_timestamp_max)
{
  types::DUMMY_FRAME_STRUCT test;

  /**
   * @test ensures the set and get functions for the timestamp works for maximum possible 64-bit unsigned integer
   */
  test.set_timestamp(UINT64_MAX);
  BOOST_REQUIRE_EQUAL(test.get_timestamp(), UINT64_MAX);

  /**
   * @test Tests setting the maximum 64-bit unsigned integer  for another_key.
   * checks by accessing it directly as there is no getter
   */
  test.set_another_key(UINT64_MAX);
  BOOST_REQUIRE_EQUAL(test.another_key, UINT64_MAX);
}

/**
 * @brief Tests less than operator
 * comparable based on composite key (timestamp + other unique keys)
 */
BOOST_AUTO_TEST_CASE(ReadoutTypes_less_than_operator)
{
  types::DUMMY_FRAME_STRUCT test1, test2;

  test1.set_timestamp(UINT64_MAX);
  test1.set_another_key(1000);

  test2.set_timestamp(UINT64_MAX);
  test2.set_another_key(500);

  BOOST_REQUIRE_EQUAL(test1 < test2, false);
  BOOST_REQUIRE_EQUAL(test2 < test1, true);
}

/**
 * @brief Tests the begin() and end() methods of DUMMY_FRAME_STRUCT.
 */
BOOST_AUTO_TEST_CASE(ReadoutTypes_begin_end)
{
  types::DUMMY_FRAME_STRUCT frame;

  /**
   * @test 'begin()' returns a pointer to the frame itself
   */
  types::DUMMY_FRAME_STRUCT* fr_begin = frame.begin();
  BOOST_REQUIRE_EQUAL(fr_begin, &frame);

  /**
   * @test 'end()' returns a pointer to one past the frame
   */
  types::DUMMY_FRAME_STRUCT* fr_end = frame.end();
  BOOST_REQUIRE_EQUAL(fr_end, &frame + 1);
}

BOOST_AUTO_TEST_SUITE_END()