//
// Copyright 2021 (c) Perun Software Ltd.
// This unpublished material is proprietary to Perun Software Ltd. All rights reserved.
// The methods and techniques described herein are considered trade secrets and/or confidential.
// Reproduction or distribution, in whole or in part, is forbidden except by express written permission of Perun Software Ltd.
//


#include "exceptions.hpp"
#include "logger.hpp"

#include <iostream>
#include <cstring>
#include <signal.h>
#include <sstream>
#include <unistd.h>

namespace networkinterface {

void log_exc(const ErrorBase& ex, const char* where) {
#if USE_STACKTRACE && BACKTRACE_SUPPORTED
  const ErrorBase* err = dynamic_cast<const ErrorBase*>(&ex);
  if(err) {
    l.e(where, err->what(), {{"tb", "\n" + err->_stacktrace}});
  } else {
    l.e(where, ex.what());
  }
#else
  Logger::getInstance().log(where + ex.what(), Logger::Severity::ERROR, "BPARSER");
//   l.e(where, ex.what());
#endif
}

void default_signal_handler(int signum) {
  // ????
}

void register_signal_handlers() {
  // ????
  // ::signal(SIGSEGV, &default_signal_handler);
  // ::signal(SIGABRT, &default_signal_handler);
}


std::string capture_st() {
#if USE_STACKTRACE && BACKTRACE_SUPPORTED
  std::stringstream ss;
  // TODO: reuse state for all threads and calls
  backtrace_full(get_bt_state(), 2, print_callback, error_callback, &ss);
  return ss.str();
#else
  return "";
#endif
}

void crash_and_burn(std::string because) {
  // TODO: log trace here
//   l.e("CRASHING", because);
  Logger::getInstance().log(because, Logger::Severity::ERROR, "CRASHING");
  kill(getpid(), 9);
  throw -1;
}

ErrorBase::ErrorBase(std::string msg)
    : _err_str(std::move(msg)) { }

NoDataException::NoDataException(std::string what)
    : ErrorBase(std::move(what)) { }

ArgumentError::ArgumentError(std::string what)
    : ErrorBase(std::move(what)) { }

ProgrammingError::ProgrammingError(std::string what)
    : ErrorBase(std::move(what)) { }

IOError::IOError(std::string what)
    : ErrorBase(std::move(what)) { }

ReadError::ReadError(std::string what)
    : IOError(std::move(what)) { }

BufferReadError::BufferReadError(std::string what)
    : ReadError(std::move(what)) { }
BufferReadError::BufferReadError(int64_t amount)
    : ReadError("read missing " + std::to_string(amount)) { }

ReadEOF::ReadEOF(std::string what)
    : ReadError(std::move(what)) { }

WriteError::WriteError(std::string what)
    : IOError(std::move(what)) { }

WriteEOF::WriteEOF(std::string what)
    : WriteError(std::move(what)) { }

BufferWriteError::BufferWriteError(std::string what)
    : WriteError(std::move(what)) { }
BufferWriteError::BufferWriteError(int64_t amount)
    : WriteError("write missing " + std::to_string(amount)) { }

ConstructClosed::ConstructClosed(std::string what)
    : ErrorBase(std::move(what)) { }

TcpNetworkError::TcpNetworkError(std::string what)
    : IOError(std::move(what)) { }

DenialOfServiceGuard::DenialOfServiceGuard(std::string what)
    : ErrorBase(std::move(what)) { }

ProtocolError::ProtocolError(std::string what)
    : ErrorBase(std::move(what)) { }

EncoderError::EncoderError(std::string what)
    : ErrorBase(std::move(what)) { }

PreconditionError::PreconditionError(std::string what)
    : ErrorBase(std::move(what)) { }

InternalError::InternalError(std::string what)
    : ErrorBase(std::move(what)) { }

ScriptingError::ScriptingError(std::string what)
    : ErrorBase(std::move(what)) { }

TimeoutError::TimeoutError(std::string what)
    : ErrorBase(std::move(what)) { }

NotYetImplemented::NotYetImplemented(std::string what)
    : ErrorBase(std::move(what)) { }

} // namespace networkinterface
