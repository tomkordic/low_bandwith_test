//
// Copyright 2021 (c) Perun Software Ltd.
// This unpublished material is proprietary to Perun Software Ltd. All rights reserved.
// The methods and techniques described herein are considered trade secrets and/or confidential.
// Reproduction or distribution, in whole or in part, is forbidden except by express written permission of Perun Software Ltd.
//

#ifndef NETWORK_MEYHAM_EXCEPTIONS_HPP_
#define NETWORK_MEYHAM_EXCEPTIONS_HPP_

// #include <exception>
#include <cstdint>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace networkinterface {

constexpr int64_t ERR_OK       = 0;
constexpr int64_t ERR_TEARDOWN = 1;

struct Err {
  int64_t     kind = ERR_TEARDOWN;
  std::string trace;
};

/*

What are errors(exceptions) used for:
- Error tracing    -> logging, error funnel
- Stack unwinding  -> connection-break causing resource cleanup for this client/component
- Data passing / Control flow -> Exiting main component loop on each broken queue

When should errors be used:
- As rarely as possible
- For unhappy path

Use as few exception types as possible.
TODO: Minimize number of exception types.

*/

// TODO: add class name here and in log_exc
struct ErrorBase {
  // TODO: uncomment when using c++23
  explicit ErrorBase(std::string str /*, const std::source_location& loc, std::stacktrace trace*/);
  std::string&       what() { return _err_str; }
  const std::string& what() const noexcept { return _err_str; }
  // const std::source_location& where() const noexcept { return _location; }
  // const std::stacktrace& stack() const noexcept { return _stacktrace; }

protected:
  std::string _err_str;
  // const std::source_location _location;
  // const std::stacktrace _stacktrace;
};

template <typename DATA, typename B = ErrorBase>
struct MakeError : B {
public:
  MakeError(std::string str)
      : ErrorBase(std::move(str)) { }
  // // TODO: uncomment when using c++23
  // MakeError(std::string str, DATA data , const std::source_location& loc=std::source_location::current(), std::stacktrace trace=std::stacktrace::current())
  //     : ErrorBase(std::move(str), loc, trace)
  //     , _data {std::move(data)} { }
  //   DATA&       data() { return _data; }
  //   const DATA& data() const noexcept { return _data; }
  // protected:
  //   DATA _data;
};

// TODO: uncomment when using c++23
// std::ostream& operator << (std::ostream&os, const std::source_location& l) {
//   os << l.file_name() << "(" << l.line() << ":" << l.column() << "),function `" << l.function_name() << "`";
//   return os;
// }
// std::ostream& operator << (std::ostream& os, const std::stacktrace& b) {
//   for(auto iter = b.begin(); iter != (b.end()-3); ++iter) {
//     os << iter->source_file() << "(" << iter->source_line() << "):" << iter->description() << "\n";
//   }
//   return os;
// }

// ---------------------------------
// TODO: Minimize number of errors. Doesn't make sense to have different Decoding/Encoding/Muxing/Composing/... That looks like component error

// Unable to recover from this
using FatalError = MakeError<void>;

// Component teardown on other end
using Teardown = MakeError<void>;

[[noreturn]] void crash_and_burn(std::string because);

// ---------------------------------
// ---------------------------------
// ---------------------------------
// ---------------------------------

struct NoDataException : ErrorBase {
  NoDataException(std::string what);
};

struct ArgumentError : ErrorBase {
  ArgumentError(std::string what);
};

struct ProgrammingError : ErrorBase {
  ProgrammingError(std::string what);
};

struct IOError : ErrorBase {
  IOError(std::string what);
};

struct ReadError : IOError {
  ReadError(std::string what);
};

struct ReadEOF : ReadError {
  ReadEOF(std::string what);
};

struct BufferReadError : ReadError {
  BufferReadError(std::string what);
  BufferReadError(int64_t amount);
};

struct WriteError : IOError {
  WriteError(std::string what);
};

struct WriteEOF : WriteError {
  WriteEOF(std::string what);
};

struct BufferWriteError : WriteError {
  BufferWriteError(std::string what);
  BufferWriteError(int64_t amount);
};

struct TcpNetworkError : IOError {
  TcpNetworkError(std::string what);
};

struct ConstructClosed : ErrorBase {
  ConstructClosed(std::string what);
};

struct DenialOfServiceGuard : ErrorBase {
  DenialOfServiceGuard(std::string what);
};

struct ProtocolError : ErrorBase {
  ProtocolError(std::string what);
};

struct EncoderError : ErrorBase {
  EncoderError(std::string what);
};

struct PreconditionError : ErrorBase {
  PreconditionError(std::string what);
};

struct InternalError : ErrorBase {
  InternalError(std::string what);
};

struct ScriptingError : ErrorBase {
  ScriptingError(std::string what);
};

struct TimeoutError : ErrorBase {
  TimeoutError(std::string what);
};

struct NotYetImplemented : ErrorBase {
  NotYetImplemented(std::string what);
};

#if 1
template <class E>
[[noreturn]] void throw_with_trace(const E& e) {
  static_assert(std::is_base_of_v<ErrorBase, E>, "E must be a subclass of ErrorBase");
  throw e;
}
#else
#define throw_with_trace(...)                                                                          \
  {                                                                                                    \
    auto              e___ = __VA_ARGS__;                                                              \
    networkinterface::ErrorBase* eb__ = static_cast<networkinterface::ErrorBase*>(&e___);                                    \
    std::stringstream s___;                                                                            \
    s___ << std::string(__FILE__) << ":" << std::to_string(__LINE__) << " " << eb__->_constructed_msg; \
    eb__->_final_msg = s___.str();                                                                     \
    throw e___;                                                                                        \
  }
#endif

void log_exc(const ErrorBase& ex, const char* where);

void register_signal_handlers();

} // namespace networkinterface

#endif // NETWORK_MEYHAM_EXCEPTIONS_HPP_
