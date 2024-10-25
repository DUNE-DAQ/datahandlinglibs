#include "datahandlinglibs/DataMoveCallbackRegistry.hpp"
#include "datahandlinglibs/ReadoutTypes.hpp"
#include "datahandlinglibs/concepts/LatencyBufferConcept.hpp"
#include "datahandlinglibs/concepts/RawDataProcessorConcept.hpp"
#include "datahandlinglibs/concepts/DataHandlingConcept.hpp"
#include "datahandlinglibs/concepts/RecorderConcept.hpp"
#include "datahandlinglibs/concepts/RequestHandlerConcept.hpp"
#include "datahandlinglibs/concepts/SourceEmulatorConcept.hpp"

#include "datahandlinglibs/models/BinarySearchQueueModel.hpp"
#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include "datahandlinglibs/models/EmptyFragmentRequestHandlerModel.hpp"
#include "datahandlinglibs/models/FixedRateQueueModel.hpp"
#include "datahandlinglibs/models/IterableQueueModel.hpp"
#include "datahandlinglibs/models/DataHandlingModel.hpp"
#include "datahandlinglibs/models/RecorderModel.hpp"
#include "datahandlinglibs/models/SkipListLatencyBufferModel.hpp"
#include "datahandlinglibs/models/DataSubscriberModel.hpp"
#include "datahandlinglibs/models/SourceEmulatorModel.hpp"
#include "datahandlinglibs/models/TaskRawDataProcessorModel.hpp"
#include "datahandlinglibs/models/ZeroCopyRecordingRequestHandlerModel.hpp"

namespace dunedaq::datahandlinglibs {

  static_assert(types::IsDataHandlingCompliantType<types::ValidDataHandlingStruct>, "ValidDataHandlingStruct does not meet the Data Handling Type requirements.");
  static_assert(types::IsDataHandlingCompliantType<types::DUMMY_FRAME_STRUCT>, "DUMMY_FRAME_STRUCT does not meet the Data Handling Type requirements.");

}
