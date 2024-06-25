/**
 * @file DataMoveCallbackRegistry.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2024.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "datahandlinglibs/DataMoveCallbackRegistry.hpp"

#include <memory>

std::shared_ptr<dunedaq::datahandlinglibs::DataMoveCallbackRegistry> dunedaq::datahandlinglibs::DataMoveCallbackRegistry::s_instance = nullptr;

