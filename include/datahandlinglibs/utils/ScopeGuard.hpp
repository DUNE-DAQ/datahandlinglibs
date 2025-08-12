/**
 * @file ScopeGuard.hpp Simple ScopeGuard to run actions on scope exit 
 * (pre-std::experimental::scope_exit)
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_UTILS_SCOPEGUARD_HPP_
#define DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_UTILS_SCOPEGUARD_HPP_

#include <utility>

namespace dunedaq {
namespace datahandlinglibs {

// -----------------------------------------------------------------------------
// ScopeGuard: A simple RAII helper to run a function when going out of scope.
// Simple ScopeGuard to run actions on scope exit (pre std::experimental::scope_exit)
// 
// Purpose:
//   Ensures that a specified action (usually resetting state or releasing
//   resources) runs automatically when the ScopeGuard object is destroyed, no matter
//   how the scope is exited (normal return, exception thrown, etc).
//
// Usage:
//   Instantiate with a lambda or function to run on destruction.
//   If the action should not run (e.g., manual dismissal), call dismiss().
//
// Characteristics:
//   - Non-copyable to prevent accidental multiple calls.
//   - Runs the function exactly once, when the object is destroyed.
// -----------------------------------------------------------------------------
template<typename F>
class ScopeGuard {
  F func_;       // The function to run 
  bool active_;  // Flag to control whether the function should run

public:
  // Constructor takes a function (e.g. a lambda) and stores it
  explicit ScopeGuard(F&& func) : func_(std::move(func)), active_(true) {}
  
  // Destructor runs the function if active_ is still true
  ~ScopeGuard() { if (active_) func_(); }

  // Manually disable the action (e.g., if it was done early)
  void dismiss() { active_ = false; }

  // Prevent copying to avoid double calls
  ScopeGuard(const ScopeGuard&) = delete;
  ScopeGuard& operator=(const ScopeGuard&) = delete;
};

} // namespace datahandlinglibs
} // namespace dunedaq

#endif // DATAHANDLINGLIBS_INCLUDE_DATAHANDLINGLIBS_UTILS_SCOPEGUARD_HPP_
