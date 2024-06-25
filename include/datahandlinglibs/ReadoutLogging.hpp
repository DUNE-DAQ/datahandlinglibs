/**
 * @file ReadoutLogging.hpp Common logging declarations in datahandlinglibs
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_READOUTLOGGING_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_READOUTLOGGING_HPP_

namespace dunedaq {
namespace datahandlinglibs {
namespace logging {

/**
 * @brief Common name used by TRACE TLOG calls from this package
 */
enum
{
  TLVL_HOUSEKEEPING = 1,
  TLVL_TAKE_NOTE = 2,
  TLVL_ENTER_EXIT_METHODS = 5,
  TLVL_WORK_STEPS = 10,
  TLVL_QUEUE_PUSH = 11,
  TLVL_QUEUE_POP = 12,
  TLVL_BOOKKEEPING = 15,
  TLVL_TIME_SYNCS = 17,
  TLVL_FRAME_RECEIVED = 19
};

} // namespace logging
} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_READOUTLOGGING_HPP_
