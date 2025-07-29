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
#define BOOST_TEST_MODULE datahandlinglibs_IterableQueueModel_test // NOLINT

#include "boost/test/unit_test.hpp"

#include "datahandlinglibs/models/SkiplistLatencyBufferModel.hpp"
#include "datahandlinglibs/ReadoutTypes.hpp"

using namespace dunedaq::datahandlinglibs;

BOOST_AUTO_TEST_SUITE(datahandlinglibs_SkiplistLatencyBufferModel_test)

/**
 * @brief Tests is_Empty, capacity, read, write, isFull functions
 */
BOOST_AUTO_TEST_CASE(SkiplistLatencyBufferModel_basic_queue_functionality)
{
    SkipListLatencyBufferModel <types::DUMMY_FRAME_STRUCT> skip_list;


}