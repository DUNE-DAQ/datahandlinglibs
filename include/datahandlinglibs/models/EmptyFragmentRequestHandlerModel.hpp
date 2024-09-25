/**
 * @file EmptyFragmentRequestHandlerModel.hpp Request handler that always returns 
 * empty fragments, mainly used for debugging purposes.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_EMPTYFRAGMENTREQUESTHANDLERMODEL_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_EMPTYFRAGMENTREQUESTHANDLERMODEL_HPP_

#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"

#include <memory>
#include <utility>
#include <vector>

using dunedaq::datahandlinglibs::logging::TLVL_HOUSEKEEPING;
using dunedaq::datahandlinglibs::logging::TLVL_QUEUE_PUSH;
using dunedaq::datahandlinglibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace datahandlinglibs {

template<class ReadoutType, class LatencyBufferType>
class EmptyFragmentRequestHandlerModel : public DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>
{
public:
  // Using inherited typenames
  using inherited = DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>;
  using RequestResult =
    typename dunedaq::datahandlinglibs::RequestHandlerConcept<ReadoutType, LatencyBufferType>::RequestResult;
  using ResultCode =
    typename dunedaq::datahandlinglibs::RequestHandlerConcept<ReadoutType, LatencyBufferType>::ResultCode;


  // Explicit constructor to bind LB and error registry
  explicit EmptyFragmentRequestHandlerModel(std::shared_ptr<LatencyBufferType>& latency_buffer,
                                            std::unique_ptr<FrameErrorRegistry>& error_registry)
    : DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>(latency_buffer, error_registry)
  {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "EmptyFragmentRequestHandlerModel created...";
  }

  // Override the issue_request implementation of the DefaultRequestHandlerModel
  // in order to always respond with empty fragments. 
  void issue_request(dfmessages::DataRequest datarequest, bool is_retry=false) override;

};

} // namespace datahandlinglibs
} // namespace dunedaq

// Declarations
#include "detail/EmptyFragmentRequestHandlerModel.hxx"

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_EMPTYFRAGMENTREQUESTHANDLERMODEL_HPP_
