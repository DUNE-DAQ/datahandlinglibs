/**
 * @file UnitTestUtilities.hpp Unit test helper classes
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP

#include "datahandlinglibs/models/DataHandlingModel.hpp"
#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/models/TaskRawDataProcessorModel.hpp"

namespace dunedaq {
namespace datahandlinglibs {
namespace unittest {

template<typename ReadoutType,
         typename RequestHandlerType,
         typename LatencyBufferType,
         typename RawDataProcessorType,
         typename InputDataType = ReadoutType>
class MockDataHandlingModel
  : public DataHandlingModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType, InputDataType>
{
public:
  using Base =
    DataHandlingModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType, InputDataType>;
  using Base::Base;
  using Base::PostprocessScheduleAlgorithm;
};

} // namespace unittest
} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP
