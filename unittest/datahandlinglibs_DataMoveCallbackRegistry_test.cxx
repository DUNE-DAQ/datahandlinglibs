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
   * @test callback is already registere
   */
  /*
  std::ostringstream captured_output; 
  // Save original buffer 
  auto* old_buf = std::clog.rdbuf(); 
  std::clog.rdbuf(captured_output.rdbuf()); 
  // Redirect   std::clog // Call the function that may log via TLOG() 
  registry->register_callback<int>("id", [](int&&) {});
  // Restore original buffer 
  std::clog.rdbuf(old_buf); 
  // Check if expected log message appears 
  std::string logs = captured_output.str(); 
  BOOST_CHECK(logs.find("Callback is already registered with ID: id" ) != std::string::npos);
  */

  

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
