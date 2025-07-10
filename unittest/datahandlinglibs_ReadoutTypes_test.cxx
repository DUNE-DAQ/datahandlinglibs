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


BOOST_AUTO_TEST_CASE(ReadoutTypes_get_set_timestamp_max)
{
    types::DUMMY_FRAME_STRUCT test;
    
    test.set_timestamp(UINT64_MAX);
    test.set_another_key(UINT64_MAX);

    BOOST_REQUIRE_EQUAL(test.get_timestamp(),UINT64_MAX);
    BOOST_REQUIRE_EQUAL(test.another_key,UINT64_MAX);
}


BOOST_AUTO_TEST_CASE(ReadoutTypes_less_than_operator)
{
    types::DUMMY_FRAME_STRUCT test1, test2;
    
    test1.set_timestamp(UINT64_MAX);
    test1.set_another_key(1000);

    test2.set_timestamp(UINT64_MAX);
    test2.set_another_key(500);

    BOOST_REQUIRE_EQUAL(test1 < test2,false);
    BOOST_REQUIRE_EQUAL(test2 < test1,true);
}


BOOST_AUTO_TEST_CASE(ReadoutTypes_begin_end)
{
    types::DUMMY_FRAME_STRUCT frame;

    types::DUMMY_FRAME_STRUCT* fr_begin = frame.begin();
    types::DUMMY_FRAME_STRUCT* fr_end = frame.end();

    BOOST_REQUIRE_EQUAL(fr_begin, &frame);
    BOOST_REQUIRE_EQUAL(fr_end, &frame + 1);
}


BOOST_AUTO_TEST_SUITE_END()