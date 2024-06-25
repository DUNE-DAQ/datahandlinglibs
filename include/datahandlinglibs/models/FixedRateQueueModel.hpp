/**
 * @file FixedRateQueueModel.hpp Queue that can be searched
 *
 * This is part of the DUNE DAQ , copyright 2021.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_FIXEDRATEQUEUEMODEL_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_FIXEDRATEQUEUEMODEL_HPP_

#include "datahandlinglibs/DataHandlingIssues.hpp"
#include "datahandlinglibs/ReadoutLogging.hpp"

#include "logging/Logging.hpp"

#include "BinarySearchQueueModel.hpp"

namespace dunedaq {
namespace datahandlinglibs {

template<class T>
class FixedRateQueueModel : public BinarySearchQueueModel<T>
{
public:
  FixedRateQueueModel()
    : BinarySearchQueueModel<T>()
  {}

  explicit FixedRateQueueModel(uint32_t size) // NOLINT(build/unsigned)
    : BinarySearchQueueModel<T>(size)
  {}

  typename IterableQueueModel<T>::Iterator lower_bound(T& element, bool with_errors = false);

};

} // namespace datahandlinglibs
} // namespace dunedaq

// Declarations
#include "detail/FixedRateQueueModel.hxx"

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_MODELS_FIXEDRATEQUEUEMODEL_HPP_
