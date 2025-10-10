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
  : public 
      DataHandlingModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType, InputDataType>
{
public:
  using Base = 
    DataHandlingModel<ReadoutType, RequestHandlerType, LatencyBufferType, RawDataProcessorType, InputDataType>;
  using Base::Base;
  using Base::PostprocessManager;
};

}
}
}

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_TESTUTILS_UNITTESTUTILITIES_HPP
