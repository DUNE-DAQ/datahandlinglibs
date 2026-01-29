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
#include "serialization/Serialization.hpp"

#include "boost/test/unit_test.hpp"

using namespace dunedaq::datahandlinglibs;

DUNE_DAQ_TYPESTRING(int, "int");
DUNE_DAQ_TYPESTRING(double, "double");

BOOST_AUTO_TEST_SUITE(datahandlinglibs_DataMoveCallbackRegistry_test)

/**
 * @brief Tests for DataMoveCallbackRegistry singleton
 */
BOOST_AUTO_TEST_CASE(DataMoveCallbackRegistry_get_callback)
{
  auto confdb = std::make_shared<dunedaq::conffwk::Configuration>("oksconflibs:test/config/datahandlinglibs_DataMoveCallbackRegistry_test.data.xml");
  auto conf = confdb->get<dunedaq::appmodel::RawDataCallbackConf>("id");

  auto registry = DataMoveCallbackRegistry::get();

  /**
   * @test No callback function registered with the given ID
   */
  BOOST_CHECK(registry->get_callback<int>(conf) == nullptr);

  /**
   * @test A registered callback function can be retrieved
   */
  registry->register_callback<int>(conf, [](int&&) {});
  BOOST_CHECK(registry->get_callback<int>(conf) != nullptr);

  /**
   * @test The registered callback function's parameter type and template argument must match
   */
  BOOST_CHECK_THROW(registry->get_callback<double>(conf), GenericConfigurationError);
}

BOOST_AUTO_TEST_SUITE_END()
