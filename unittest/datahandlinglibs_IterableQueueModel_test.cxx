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

#include "datahandlinglibs/models/IterableQueueModel.hpp"

#include "appmodel/LatencyBuffer.hpp"
#include "conffwk/DalRegistry.hpp"
#include "conffwk/Configuration.hpp"

#include <iostream>


using namespace dunedaq::datahandlinglibs;

BOOST_AUTO_TEST_SUITE(datahandlinglibs_IterableQueueModel_test)

/**
 * @brief Tests is_Empty, capacity, read, write, isFull functions
 */
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

/**
 * @brief Tests various pop operations and makes sure size and capacity remain consistent
 */
BOOST_AUTO_TEST_CASE(IterableQueueModel_pops_and_sizes)
{
    int size = 11;
    IterableQueueModel<int> queue(size,  false,0,false,0);

    //should not change
    int original_capacity = queue.capacity();
    int original_size = queue.size();

    for (int i = 0; i < original_capacity ; i++)
    {
        queue.write(std::move(i));
    }
    BOOST_REQUIRE(queue.isFull());

    int original_occupancy = queue.occupancy();

    BOOST_REQUIRE_EQUAL(original_occupancy, original_capacity);
    BOOST_REQUIRE_EQUAL(original_capacity, original_size-1);
    BOOST_REQUIRE_EQUAL(queue.size(), original_size);

    int value_in_queue;
    bool read_successful = queue.read(value_in_queue);    
    BOOST_REQUIRE_EQUAL(queue.occupancy(),original_occupancy-1);
    BOOST_REQUIRE_EQUAL(queue.capacity(),original_capacity);
    BOOST_REQUIRE_EQUAL(queue.size(), original_size);
    BOOST_REQUIRE(!queue.isFull());

    //after full
    bool write_successful = queue.write(std::move(original_capacity));
    BOOST_REQUIRE(write_successful);
    BOOST_REQUIRE(queue.isFull());
    BOOST_REQUIRE_EQUAL(queue.capacity() ,original_capacity);
    BOOST_REQUIRE_EQUAL(queue.occupancy(), original_capacity);
    BOOST_REQUIRE_EQUAL(queue.size(), original_size);
    BOOST_CHECK_EQUAL(queue.back(),queue.start_of_buffer());
    
    queue.popFront();
    BOOST_REQUIRE_EQUAL(queue.occupancy(),original_occupancy-1);
    BOOST_REQUIRE_EQUAL(queue.capacity() ,original_capacity);
    BOOST_REQUIRE_EQUAL(queue.size(), original_size);

    queue.pop(3);
    BOOST_REQUIRE_EQUAL(queue.occupancy(),original_occupancy-4);
    BOOST_REQUIRE_EQUAL(queue.capacity() ,original_capacity);
    BOOST_REQUIRE_EQUAL(queue.size(), original_size);

    queue.flush();
    BOOST_REQUIRE_EQUAL(queue.occupancy(),0);
    BOOST_REQUIRE_EQUAL(queue.capacity() ,original_capacity);
    BOOST_REQUIRE_EQUAL(queue.size(), original_size);
    BOOST_REQUIRE(queue.isEmpty());
}

/**
 * @brief Tests force pagefault function
 */
BOOST_AUTO_TEST_CASE(IterableQueueModel_force_pagefault)
{
    IterableQueueModel<int> queue(6,  false,0,false,0);

    queue.write(1);
    BOOST_REQUIRE(!queue.isEmpty());
    queue.force_pagefault();
    BOOST_REQUIRE(queue.isEmpty());
}

/**
 * @brief Tests mamory allocation
 */
BOOST_AUTO_TEST_CASE(IterableQueueModel_allocate_deallocate_memory)
{
    //default queue
    IterableQueueModel<int> default_queue;

    //!numa_aware && !intrinsic_allocator && alignment_size == 0
    IterableQueueModel<int> queue(2,false,0,false,0);
    BOOST_REQUIRE_EQUAL( queue.size(), 2);
    queue.free_memory();

    //intrinsic_allocator && alignment_size > 0
    queue.allocate_memory(16,false,0,true,2);
    BOOST_REQUIRE_EQUAL( queue.size(), 16);
    BOOST_REQUIRE_EQUAL( queue.get_alignment_size(), 2);
    queue.free_memory();

    //!intrinsic_allocator && alignment_size > 0
    queue.allocate_memory(8,false,0,false,4);

    queue.write(0);
    queue.write(2);
    int value;
    queue.read(value);
    BOOST_REQUIRE_EQUAL(value, 0);
    queue.read(value);
    BOOST_REQUIRE_EQUAL(value, 2);

    BOOST_REQUIRE_EQUAL( queue.size(), 8);
    BOOST_REQUIRE_EQUAL( queue.get_alignment_size(), 4);
    
    //numa_aware && numa_node < 8 
    queue.allocate_memory(64,true,4,false,0);
    BOOST_REQUIRE_EQUAL( queue.size(), 64);
    queue.free_memory();

    queue.allocate_memory(42);
    BOOST_REQUIRE_EQUAL( queue.size(), 42);

    queue.write(0);
    queue.read(value);
    BOOST_REQUIRE_EQUAL(value, 0);

}

/**
 * @brief Tests queue pointers after write and read operations
 */
BOOST_AUTO_TEST_CASE(IterableQueueModel_queue_pointers)
{
    IterableQueueModel<int> queue(5,false,0,false,0);
    queue.write(0);
    queue.write(1);
    queue.write(2);
    queue.write(3);

    BOOST_REQUIRE_EQUAL(*queue.front(), 0);
    BOOST_REQUIRE_EQUAL(*queue.back(), 3);

    BOOST_CHECK_EQUAL(queue.front(), queue.start_of_buffer());
    BOOST_CHECK_EQUAL(queue.back()+2, queue.end_of_buffer());

    int value;
    queue.read(value);
    BOOST_REQUIRE_EQUAL(*queue.front(), 1);
    BOOST_REQUIRE_EQUAL(queue.front(), queue.start_of_buffer()+1);

    queue.read(value);
    queue.read(value);
    queue.read(value);
}

/**
 * @brief Tests functionality for iterators
 */
BOOST_AUTO_TEST_CASE(IterableQueueModel_iterators)
{
    IterableQueueModel<int> queue(5, false, 0, false, 0);
    queue.write(0);
    queue.write(1);
    queue.write(2);
    queue.write(3);

    auto it = queue.begin();
    int last_value = -1;

    while (it != queue.end()) {
        last_value = *it;
        ++it;
    }

    BOOST_REQUIRE(it == queue.end()); 
    BOOST_REQUIRE_EQUAL(last_value, *queue.back());

}
BOOST_AUTO_TEST_SUITE_END()