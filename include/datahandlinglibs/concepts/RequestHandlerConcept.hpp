/**
 * @file RequestHandlerConcept.hpp RequestHandler interface class
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_CONCEPTS_REQUESTHANDLERCONCEPT_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_CONCEPTS_REQUESTHANDLERCONCEPT_HPP_

#include "iomanager/IOManager.hpp"
#include "daqdataformats/Fragment.hpp"
#include "dfmessages/DataRequest.hpp"
#include "appmodel/DataHandlerModule.hpp"
#include "opmonlib/MonitorableObject.hpp"

#include <map>
#include <memory>
#include <string>
#include <utility>

namespace dunedaq {
namespace datahandlinglibs {

template<class ReadoutType, class LatencyBufferType>
class RequestHandlerConcept : public opmonlib::MonitorableObject
{

public:
  RequestHandlerConcept() {}

  virtual ~RequestHandlerConcept() {}
  
  RequestHandlerConcept(const RequestHandlerConcept&) = delete; ///< RequestHandlerConcept is not copy-constructible
  RequestHandlerConcept& operator=(const RequestHandlerConcept&) =
    delete;                                                ///< RequestHandlerConcept is not copy-assginable
  RequestHandlerConcept(RequestHandlerConcept&&) = delete; ///< RequestHandlerConcept is not move-constructible
  RequestHandlerConcept& operator=(RequestHandlerConcept&&) = delete; ///< RequestHandlerConcept is not move-assignable

  //virtual void init(const nlohmann::json& args) = 0;
  virtual void conf(const appmodel::DataHandlerModule* conf) = 0;
  virtual void scrap(const nlohmann::json& args) = 0;
  virtual void start(const nlohmann::json& args) = 0;
  virtual void stop(const nlohmann::json& args) = 0;
  virtual void record(const nlohmann::json& args) = 0;

  //! Check if cleanup is necessary and execute it if necessary
  virtual void cleanup_check() = 0;

  //! Periodic data transmission - relevant for trigger in particular
  virtual void periodic_data_transmission() = 0;

  //! Issue a data request to the request handler
  virtual void issue_request(dfmessages::DataRequest /*dr*/, bool /*is_retry*/) = 0;


protected:
  // Result code of requests
  enum ResultCode
  {
    kFound = 0,
    kNotFound,
    kTooOld,
    kNotYet,
    kPartial,
    kPartiallyOld,
    kCleanup,
    kUnknown
  };
  std::map<ResultCode, std::string> ResultCodeStrings{
    { ResultCode::kFound, "FOUND" },    { ResultCode::kNotFound, "NOT_FOUND" },
    { ResultCode::kTooOld, "TOO_OLD" }, { ResultCode::kNotYet, "NOT_YET_PRESENT" },
    { ResultCode::kPartiallyOld, "PARTIALLY_PASSED" },    { ResultCode::kCleanup, "CLEANUP" },
    { ResultCode::kUnknown, "UNKNOWN" }
  };

  inline const std::string& resultCodeAsString(ResultCode rc) { return ResultCodeStrings[rc]; }

  // Request Result
  struct RequestResult
  {
    RequestResult(ResultCode rc, dfmessages::DataRequest dr)
      : result_code(rc)
      , data_request(dr)
      , fragment()
    {}
    RequestResult(ResultCode rc, dfmessages::DataRequest dr, daqdataformats::Fragment&& frag)
      : result_code(rc)
      , data_request(dr)
      , fragment(std::move(frag))
    {}
    ResultCode result_code;
    dfmessages::DataRequest data_request;
    std::unique_ptr<daqdataformats::Fragment> fragment;
  };

  virtual void cleanup() = 0;
  virtual RequestResult data_request(dfmessages::DataRequest /*dr*/) = 0;

private:
};

} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_CONCEPTS_REQUESTHANDLERCONCEPT_HPP_
