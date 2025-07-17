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
#define BOOST_TEST_MODULE datahandlinglibs_IterableQueueModel_test // NOLINT

#include "boost/test/unit_test.hpp"

#include "datahandlinglibs/models/IterableQueueModel.hpp"

#include <iostream>


using namespace dunedaq::datahandlinglibs;

BOOST_AUTO_TEST_SUITE(datahandlinglibs_IterableQueueModel_test)

BOOST_AUTO_TEST_CASE(IterableQueueModel_basic_queue_functionality)
{
    int size = 11;
    IterableQueueModel<int> queue(size,  false,0,false,0);

    BOOST_REQUIRE(queue.isEmpty());

    int capacity = queue.capacity();

    bool write_successful = false;
    for (int i = 0; i < capacity ; i++)
    {
        write_successful = queue.write(std::move(i));
        BOOST_REQUIRE(write_successful);
    }

    write_successful = queue.write(10);
    BOOST_REQUIRE(!write_successful);

    BOOST_REQUIRE(queue.isFull());

    bool read_successful = false;  
    int value_in_queue;
    for (int i = 0; i < capacity; i++)
    {
        read_successful = queue.read(value_in_queue); 
        BOOST_REQUIRE(read_successful);
        BOOST_REQUIRE_EQUAL(i,value_in_queue);
    }
    BOOST_REQUIRE(queue.isEmpty());
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_pops_and_sizes)
{
    int size = 11;
    IterableQueueModel<int> queue(size,  false,0,false,0);

    //should not change
    int original_capacity = queue.capacity();
    int original_size = queue.size();


    for (int i = 0; i < size ; i++)
    {
        queue.write(std::move(i));
    }
    BOOST_REQUIRE(queue.isFull());


    int original_occupancy = queue.occupancy();

    BOOST_REQUIRE_EQUAL(original_occupancy, original_capacity);
    BOOST_REQUIRE_EQUAL(original_capacity, original_size-1);
    BOOST_REQUIRE_EQUAL(size, original_size);

    //read size check
    int value_in_queue;
    queue.read(value_in_queue);
    BOOST_REQUIRE_EQUAL(queue.occupancy(),original_occupancy-1);
    BOOST_REQUIRE_EQUAL(original_capacity, original_size-1);
    BOOST_REQUIRE_EQUAL(size, original_size);

    //popFront check
    queue.popFront();
    BOOST_REQUIRE_EQUAL(queue.occupancy(),original_occupancy-2);
    BOOST_REQUIRE_EQUAL(original_capacity, original_size-1);
    BOOST_REQUIRE_EQUAL(size, original_size);

    //pop check
    queue.pop(3);
    BOOST_REQUIRE_EQUAL(queue.occupancy(),original_occupancy-5);
    BOOST_REQUIRE_EQUAL(original_capacity, original_size-1);
    BOOST_REQUIRE_EQUAL(size, original_size);

    //flush check
    queue.flush();
    BOOST_REQUIRE_EQUAL(queue.occupancy(),0);
    BOOST_REQUIRE_EQUAL(original_capacity, original_size-1);
    BOOST_REQUIRE_EQUAL(size, original_size);
    BOOST_REQUIRE(queue.isEmpty());

}


BOOST_AUTO_TEST_SUITE_END()