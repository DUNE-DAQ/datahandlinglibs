/**
 * @file datahandlinglibs_DataMoveCallbackRegistry_test.cxx Unit tests for DataMoveCallbackRegistry
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

/**
 * @brief Name of this test module
 */
#define BOOST_TEST_MODULE datahandlinglibs_DataMoveCallbackRegistry_test // NOLINT

#include "datahandlinglibs/DataMoveCallbackRegistry.hpp"

#include "boost/test/unit_test.hpp"

#include <iostream>

#include <sstream>

using namespace dunedaq::datahandlinglibs;

BOOST_AUTO_TEST_SUITE(datahandlinglibs_DataMoveCallbackRegistry_test)

/**
 * @brief Tests for DataMoveCallbackRegistry singleton
 */
BOOST_AUTO_TEST_CASE(DataMoveCallbackRegistry_get_callback)
{
  auto registry = DataMoveCallbackRegistry::get();

  /**
   * @test No callback function registered with the given ID
   */
  BOOST_CHECK(registry->get_callback<int>("id") == nullptr);

  /**
   * @test A registered callback function can be retrieved
   */
  registry->register_callback<int>("id", [](int&&) {});
  BOOST_CHECK(registry->get_callback<int>("id") != nullptr);

  /**
   * @test The registered callback function's parameter type and template argument must match
   */
  BOOST_CHECK_THROW(registry->get_callback<double>("id"), GenericConfigurationError);

  /**
   * @test Returned function must work
   */
  int check;
  registry->register_callback<int>("cout", [&check](int&& num) {check = num;});
  std::shared_ptr<std::function<void(int&&)>> returned_func = registry->get_callback<int>("cout");
  (*returned_func)(42);
  BOOST_CHECK_EQUAL(check, 42);

  
}

BOOST_AUTO_TEST_SUITE_END()
