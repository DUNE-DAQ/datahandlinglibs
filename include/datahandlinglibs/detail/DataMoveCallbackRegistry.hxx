#include "datahandlinglibs/DataHandlingIssues.hpp"

#include <functional>

namespace dunedaq {

namespace datahandlinglibs {

template<typename DataType>
inline void
DataMoveCallbackRegistry::register_callback(const std::string& id, std::function<void(DataType&&)> callback) {
  if (m_callback_map.count(id) == 0) {
    TLOG() << "Registering DataMoveCallback with ID: " << id;
    m_callback_map[id] = std::make_shared<DataMoveCallback<DataType>>(DataMoveCallback(id, callback));
  } else {
    TLOG() << "Callback is already registered with ID: " << id << " Ignoring this registration.";
  }
}

template<typename DataType>
inline std::shared_ptr<std::function<void(DataType&&)>>
DataMoveCallbackRegistry::get_callback(const std::string& id)
{
  if (m_callback_map.contains(id)) {
    TLOG() << "Providing DataMoveCallback with ID: " << id;
    if (auto callback = dynamic_cast<DataMoveCallback<DataType>*>(m_callback_map[id].get()); callback != nullptr) {
      return callback->m_callback;
    } else {
      // As this can happen due to a very bad config error and wherever the acquire is called will lead to undefined behavior
      throw GenericConfigurationError(
        ERS_HERE,
        std::format("Callback cast failed for function signature with datatypes (expected vs. registered) : {} vs. {}",
                    typeid(DataMoveCallback<DataType>).name(),
                    typeid(*m_callback_map[id]).name()));
    }
  } else {
    TLOG() << "No callback registered with ID: " << id << " Returning nullptr.";
    return nullptr;
  }
}

}
}
