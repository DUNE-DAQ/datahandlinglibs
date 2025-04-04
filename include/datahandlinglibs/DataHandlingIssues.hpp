/**
 * @file DataHandlingIssues.hpp Readout system related ERS issues
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_READOUTISSUES_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_READOUTISSUES_HPP_

#include "daqdataformats/SourceID.hpp"
#include "daqdataformats/Types.hpp"

#include <ers/Issue.hpp>
#include "logging/Logging.hpp" // NOTE: if ISSUES ARE DECLARED BEFORE include logging/Logging.hpp, TLOG_DEBUG<<issue wont work.

#include <string>

namespace dunedaq {
ERS_DECLARE_ISSUE(datahandlinglibs,
                  InternalError,
                  "SourceID[" << sourceid << "] Internal Error: " << error,
                  ((daqdataformats::SourceID)sourceid)((std::string)error))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  CommandError,
                  "SourceID[" << sourceid << "] Command Error: " << commanderror,
                  ((daqdataformats::SourceID)sourceid)((std::string)commanderror))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  InitializationError,
                  "Readout Initialization Error: " << initerror,
                  ((std::string)initerror))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  ConfigurationError,
                  "SourceID[" << sourceid << "] Readout Configuration Error: " << conferror,
                  ((daqdataformats::SourceID)sourceid)((std::string)conferror))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  BufferedReaderWriterConfigurationError,
                  "Configuration Error: " << conferror,
                  ((std::string)conferror))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  DataRecorderConfigurationError,
                  "Configuration Error: " << conferror,
                  ((std::string)conferror))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  GenericConfigurationError,
                  "Configuration Error: " << conferror,
                  ((std::string)conferror))


ERS_DECLARE_ISSUE(datahandlinglibs,
                  TimeSyncTransmissionFailed,
                  "SourceID " << sourceid << " failed to send send TimeSync message to " << dest << ".",
                  ((daqdataformats::SourceID)sourceid)((std::string)dest))

ERS_DECLARE_ISSUE(datahandlinglibs, CannotOpenFile, "Couldn't open binary file: " << filename, ((std::string)filename))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  BufferedReaderWriterCannotOpenFile,
                  "Couldn't open file: " << filename,
                  ((std::string)filename))

ERS_DECLARE_ISSUE_BASE(datahandlinglibs,
                       CannotReadFile,
                       datahandlinglibs::ConfigurationError,
                       " Couldn't read properly the binary file: " << filename << " Cause: " << errorstr,
                       ((daqdataformats::SourceID)sourceid)((std::string)filename),
                       ((std::string)errorstr))

ERS_DECLARE_ISSUE(datahandlinglibs, CannotWriteToFile, "Could not write to file: " << filename, ((std::string)filename))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  PostprocessingNotKeepingUp,
                  "SourceID[" << sourceid << "] Postprocessing has too much backlog, thread: " << i,
                  ((daqdataformats::SourceID)sourceid)((size_t)i))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  EmptySourceBuffer,
                  "SourceID[" << sourceid << "] Source Buffer is empty, check file: " << filename,
                  ((daqdataformats::SourceID)sourceid)((std::string)filename))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  CannotReadFromQueue,
                  "SourceID[" << sourceid << "] Failed attempt to read from the queue: " << queuename,
                  ((daqdataformats::SourceID)sourceid)((std::string)queuename))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  CannotWriteToQueue,
                  "SourceID[" << sourceid << "] Failed attempt to write to the queue: " << queuename
                           << ". Data will be lost!",
                  ((daqdataformats::SourceID)sourceid)((std::string)queuename))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  CannotDispatch,
                  "Module [" << name << "] Failed attempt to write to the queue: "
                           << ". Data will be lost!",
                  ((std::string)name))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  RequestSourceIDMismatch,
                  "SourceID[" << sourceid << "] Got request for SourceID: " << request_sourceid,
                  ((daqdataformats::SourceID)sourceid)((daqdataformats::SourceID)request_sourceid))


ERS_DECLARE_ISSUE(datahandlinglibs,
                  TrmWithEmptyFragment,
                  "SourceID[" << sourceid << "] Trigger Matching result with empty fragment: " << trmdetails,
                  ((daqdataformats::SourceID)sourceid)((std::string)trmdetails))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  RequestOnEmptyBuffer,
                  "SourceID[" << sourceid << "] Request on empty buffer: " << trmdetails,
                  ((daqdataformats::SourceID)sourceid)((std::string)trmdetails))

ERS_DECLARE_ISSUE_BASE(datahandlinglibs,
                       FailedReadoutInitialization,
                       datahandlinglibs::InitializationError,
                       " Couldnt initialize Readout with current Init arguments " << initparams << ' ',
                       ((std::string)name),
                       ((std::string)initparams))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  FailedFakeCardInitialization,
                  "Could not initialize fake card " << name,
                  ((std::string)name))

ERS_DECLARE_ISSUE_BASE(datahandlinglibs,
                       NoImplementationAvailableError,
                       datahandlinglibs::ConfigurationError,
                       " No " << impl << " implementation available for raw type: " << rawt << ' ',
                       ((daqdataformats::SourceID)sourceid)((std::string)impl),
                       ((std::string)rawt))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  ResourceQueueError,
                  " The " << queueType << " queue was not successfully created for " << moduleName,
                  ((std::string)queueType)((std::string)moduleName))

ERS_DECLARE_ISSUE_BASE(datahandlinglibs,
                       DataRecorderModuleResourceQueueError,
                       datahandlinglibs::DataRecorderConfigurationError,
                       " The " << queueType << " queue was not successfully created. ",
                       ((std::string)name),
                       ((std::string)queueType))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  GenericResourceQueueError,
                  "The " << queueType << " queue was not successfully created for " << moduleName,
                  ((std::string)queueType)((std::string)moduleName))

ERS_DECLARE_ISSUE(datahandlinglibs, ConfigurationNote, "ConfigurationNote: " << text, ((std::string)name)((std::string)text))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  ConfigurationProblem,
                  "SourceID[" << sourceid << "] Configuration problem: " << text,
                  ((daqdataformats::SourceID)sourceid)((std::string)text))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  RequestTimedOut,
                  "SourceID[" << sourceid << "] Request timed out",
                  ((daqdataformats::SourceID)sourceid))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  VerboseRequestTimedOut,
                  "SourceID[" << sourceid << "] Request timed out for trig/seq_num " << trignum << "." << seqnum
                           << ", run_num " << runnum << ", window begin/end " << window_begin << "/" << window_end
                           << ", data_destination: " << dest,
                  ((daqdataformats::SourceID)sourceid)((daqdataformats::trigger_number_t)trignum)((daqdataformats::sequence_number_t)seqnum)((daqdataformats::run_number_t)runnum)((daqdataformats::timestamp_t)window_begin)((daqdataformats::timestamp_t)window_end)((std::string)dest))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  EndOfRunEmptyFragment,
                  "SourceID[" << sourceid << "] Empty fragment at the end of the run",
                  ((daqdataformats::SourceID)sourceid))

ERS_DECLARE_ISSUE(datahandlinglibs,
                  DataPacketArrivedTooLate,
                  "Received a late data packet in run " << run << ", payload first timestamp = " << ts1 <<
                  ", request_handler cutoff timestamp = " << ts2 << ", difference = " << tick_diff <<
                  " ticks, " << msec_diff << " msec.",
                  ((daqdataformats::run_number_t)run)((daqdataformats::timestamp_t)ts1)((daqdataformats::timestamp_t)ts2)((int64_t)tick_diff)((double)msec_diff))

} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_READOUTISSUES_HPP_
