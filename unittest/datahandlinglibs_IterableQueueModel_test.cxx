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
BOOST_AUTO_TEST_CASE(IterableQueueModel_initial_state)
{
    int size = 11;
    IterableQueueModel<int> queue(size, false, 0, false, 0);

    BOOST_REQUIRE(queue.isEmpty());
    BOOST_REQUIRE(!queue.isFull());
    BOOST_REQUIRE_EQUAL(queue.size(), size);
    BOOST_REQUIRE_EQUAL(queue.capacity(), size-1);
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_write_until_full)
{
    int size = 11;
    IterableQueueModel<int> queue(size, false, 0, false, 0);

    for (std::size_t i = 0; i < queue.capacity(); i++)
    {
        bool write_successful = queue.write(std::move(i));
        BOOST_REQUIRE(write_successful);
    }

    bool write_successful = queue.write(10);  // Attempt to write when full
    BOOST_REQUIRE(!write_successful);
    BOOST_REQUIRE(queue.isFull());
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_read_until_empty)
{
    int size = 11;
    IterableQueueModel<int> queue(size, false, 0, false, 0);

    for (std::size_t i = 0; i < queue.capacity(); i++)
    {
        queue.write(std::move(i));
    }

    for (std::size_t i = 0; i < queue.capacity(); i++)
    {
        int value_in_queue;
        bool read_successful = queue.read(value_in_queue);
        BOOST_REQUIRE(read_successful);
        BOOST_REQUIRE_EQUAL(i, value_in_queue);
    }

    BOOST_REQUIRE(queue.isEmpty());
}


/**
 * @brief Tests various pop operations and makes sure size and capacity remain consistent
 */
BOOST_AUTO_TEST_CASE(IterableQueueModel_full_write_check)
{
    int size = 11;
    IterableQueueModel<int> queue(size, false, 0, false, 0);

    for (std::size_t i = 0; i < queue.capacity(); ++i)
        queue.write(std::move(i));

    BOOST_REQUIRE(queue.isFull());
    BOOST_REQUIRE_EQUAL(queue.occupancy(), queue.capacity());
    BOOST_REQUIRE_EQUAL(queue.capacity(),size-1);
    BOOST_REQUIRE_EQUAL(queue.size(), size);
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_read_after_full_write)
{
    int size = 11;
    IterableQueueModel<int> queue(size, false, 0, false, 0);

    for (std::size_t i = 0; i < queue.capacity(); ++i)
        queue.write(std::move(i));

    int value;
    BOOST_REQUIRE(queue.read(value));
    BOOST_REQUIRE_EQUAL(queue.capacity(), size - 1);
    BOOST_REQUIRE_EQUAL(queue.size(), size);
    BOOST_REQUIRE(!queue.isFull());
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_write_after_full_write_and_read)
{
    int size = 11;
    IterableQueueModel<int> queue(size, false, 0, false, 0);

    for (std::size_t i = 0; i < queue.capacity(); ++i)
        queue.write(std::move(i));
    int value;
    queue.read(value);
    
    bool write_successful = queue.write(std::move(size-1));
    BOOST_REQUIRE(write_successful);
    BOOST_REQUIRE(queue.isFull());
    BOOST_REQUIRE_EQUAL(queue.capacity(), size - 1);
    BOOST_REQUIRE_EQUAL(queue.size(), size);
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_popFront)
{
    int size = 11;
    IterableQueueModel<int> queue(size, false, 0, false, 0);

    for (std::size_t i = 0; i < queue.capacity(); ++i)
        queue.write(std::move(i));

    int full_occupancy = queue.occupancy();

    queue.popFront();
    BOOST_REQUIRE_EQUAL(queue.occupancy(), full_occupancy - 1);
    BOOST_REQUIRE_EQUAL(queue.capacity(), size - 1);
    BOOST_REQUIRE_EQUAL(queue.size(), size);
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_popFront_and_pop_multiple)
{
    int size = 11;
    IterableQueueModel<int> queue(size, false, 0, false, 0);

    for (std::size_t i = 0; i < queue.capacity(); ++i)
        queue.write(std::move(i));

    int full_occupancy = queue.occupancy();

    queue.pop(3);
    BOOST_REQUIRE_EQUAL(queue.occupancy(), full_occupancy - 3);
    BOOST_REQUIRE_EQUAL(queue.capacity(), size - 1);
    BOOST_REQUIRE_EQUAL(queue.size(), size);
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_flush_queue)
{
    int size = 11;
    IterableQueueModel<int> queue(size, false, 0, false, 0);

    for (std::size_t i = 0; i < queue.capacity(); ++i)
        queue.write(std::move(i));

    queue.flush();

    BOOST_REQUIRE(queue.isEmpty());
    BOOST_REQUIRE_EQUAL(queue.occupancy(), 0);
    BOOST_REQUIRE_EQUAL(queue.capacity(), size - 1);
    BOOST_REQUIRE_EQUAL(queue.size(), size);
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
BOOST_AUTO_TEST_CASE(IterableQueueModel_default_constructor)
{
    IterableQueueModel<int> default_queue;
    BOOST_REQUIRE_EQUAL(default_queue.size(), 2);
    BOOST_REQUIRE(default_queue.isEmpty());
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_allocate_memory_standard_allocation)
{
    IterableQueueModel<int> queue;
    queue.allocate_memory(4, false, 0, true, 0);

    BOOST_REQUIRE_EQUAL(queue.size(), 4);
    BOOST_REQUIRE_EQUAL(queue.get_alignment_size(), 0);
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_allocate_memory_intrinsic_allocator_with_alignment)
{
    IterableQueueModel<int> queue;
    queue.allocate_memory(16, false, 0, true, 2);

    BOOST_REQUIRE_EQUAL(queue.size(), 16);
    BOOST_REQUIRE_EQUAL(queue.get_alignment_size(), 2);
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_allocate_memory_aligned_allocation_without_intrinsic_allocator)
{
    IterableQueueModel<int> queue;
    queue.allocate_memory(8, false, 0, false, 4);

    BOOST_REQUIRE_EQUAL(queue.size(), 8);
    BOOST_REQUIRE_EQUAL(queue.get_alignment_size(), 4);
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_allocate_memory_numa_aware_allocation)
{
    IterableQueueModel<int> queue;
    queue.allocate_memory(64, true, 4, false, 0);

    BOOST_REQUIRE_EQUAL(queue.size(), 64);
    BOOST_REQUIRE_EQUAL(queue.get_alignment_size(), 0);
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_allocate_memory_basic_overload)
{
    IterableQueueModel<int> queue;
    queue.allocate_memory(42);

    BOOST_REQUIRE_EQUAL(queue.size(), 42);
    BOOST_REQUIRE_EQUAL(queue.get_alignment_size(), 0);
}

BOOST_AUTO_TEST_CASE(IterableQueueModel_free_memory)
{
    IterableQueueModel<int> queue;
    queue.allocate_memory(42);
    queue.free_memory();
    queue.allocate_memory(8);

    BOOST_REQUIRE_EQUAL(queue.size(), 8);
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