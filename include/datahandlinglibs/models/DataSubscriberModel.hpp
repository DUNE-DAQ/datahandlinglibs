/**
 * @file DataSubscriberModel.hpp
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_SRC_SOURCEMODEL_HPP_
#define DATAHANDLINGLIBS_SRC_SOURCEMODEL_HPP_

#include "datahandlinglibs/concepts/SourceConcept.hpp"
#include "datahandlinglibs/opmon/datahandling_info.pb.h"

#include "iomanager/IOManager.hpp"
#include "iomanager/Sender.hpp"
#include "iomanager/Receiver.hpp"
#include "logging/Logging.hpp"
//#include "datahandlinglibs/utils/ReusableThread.hpp"

#include "confmodel/Connection.hpp"

//#include <folly/ProducerConsumerQueue.h>
//#include <nlohmann/json.hpp>

//#include <atomic>
//#include <memory>
//#include <mutex>
#include <functional>

namespace dunedaq::datahandlinglibs {

template<class PayloadType>
class DataSubscriberModel : public SourceConcept
{
public: 
  using inherited = SourceConcept;

  /**
   * @brief SourceModel Constructor
   * @param name Instance name for this SourceModel instance
   */
  DataSubscriberModel(): SourceConcept() {}
  ~DataSubscriberModel() {}

  void init(const confmodel::DaqModule* cfg) override {
    if (cfg->get_outputs().size() != 1) {
      throw datahandlinglibs::InitializationError(ERS_HERE, "Only 1 output supported for subscribers");
    }
    m_data_sender = get_iom_sender<PayloadType>(cfg->get_outputs()[0]->UID());

    if (cfg->get_inputs().size() != 1) {
      throw datahandlinglibs::InitializationError(ERS_HERE, "Only 1 input supported for subscribers");
    }
    m_data_receiver = get_iom_receiver<PayloadType>(cfg->get_inputs()[0]->UID());
  }

  void start() {
    m_packets = 0;
    m_sum_packets = 0;
    m_dropped_packets = 0;
    m_data_receiver->add_callback(std::bind(&DataSubscriberModel::handle_payload, this, std::placeholders::_1));
  }  

  void stop() {
    m_data_receiver->remove_callback();
  }

  bool handle_payload(PayloadType&  message) // NOLINT(build/unsigned)
  {
    ++m_packets;
    ++m_sum_packets;
    if (!m_data_sender->try_send(std::move(message), iomanager::Sender::s_no_block)) {
      ++m_dropped_packets;
    }
    return true;
  }
protected:
  virtual void generate_opmon_data() override {
   opmon::DataSourceInfo info;
   info.set_num_packets(m_packets.exchange(0));
   info.set_sum_packets(m_sum_packets) ;
   info.set_num_dropped_packets(m_dropped_packets.exchange(0));
   
   this->publish(std::move(info));
  }

private:
  using source_t = dunedaq::iomanager::ReceiverConcept<PayloadType>;
  std::shared_ptr<source_t> m_data_receiver;

  using sink_t = dunedaq::iomanager::SenderConcept<PayloadType>;
  std::shared_ptr<sink_t> m_data_sender;

  std::atomic<uint64_t> m_packets{0};
  std::atomic<uint64_t> m_sum_packets{0};
  std::atomic<uint64_t> m_dropped_packets{0};
};

} // namespace dunedaq::datahandlinglibs

#endif // DATAHANDLINGLIBS_SRC_SOURCEMODEL_HPP_
