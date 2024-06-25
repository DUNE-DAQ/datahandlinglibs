/**
 * @file ZeroCopyRecordingRequestHandlerModel.hpp Special way of recording
 * directly from memory aligned LatencyBuffers, avoiding any additional
 * copy needed to intermediate buffers (via the BufferedFileWriter)
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_ZEROCOPYRECORDINGREQUESTHANDLERMODEL_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_ZEROCOPYRECORDINGREQUESTHANDLERMODEL_HPP_

#include "datahandlinglibs/models/DefaultRequestHandlerModel.hpp"
#include <memory>

namespace dunedaq {
namespace datahandlinglibs {

template<class ReadoutType, class LatencyBufferType>
class ZeroCopyRecordingRequestHandlerModel : public DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>
{
public:
  // Using inherited typename
  using inherited = DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>;

  // Explicit constructor for binding LB and error registry
  explicit ZeroCopyRecordingRequestHandlerModel(std::unique_ptr<LatencyBufferType>& latency_buffer,
                                                std::unique_ptr<FrameErrorRegistry>& error_registry)
    : DefaultRequestHandlerModel<ReadoutType, LatencyBufferType>(latency_buffer, error_registry)
  {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "ZeroCopyRecordingRequestHandlerModel created...";
  }

  // Special configuration that checks LB alignment and O_DIRECT flag on output file
  void conf(const appmodel::DataHandlerModule* conf) override;

  // Special record command that writes to files from memory aligned LBs
  void record(const nlohmann::json& args) override;

private:
  int m_fd;
  int m_oflag;
};

} // namespace datahandlinglibs
} // namespace dunedaq

// Declarations
#include "detail/ZeroCopyRecordingRequestHandlerModel.hxx"

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_ZEROCOPYRECORDINGREQUESTHANDLERMODEL_HPP_
