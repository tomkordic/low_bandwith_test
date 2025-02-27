//
// Copyright 2021 (c) Perun Software Ltd.
// This unpublished material is proprietary to Perun Software Ltd. All rights reserved.
// The methods and techniques described herein are considered trade secrets and/or confidential.
// Reproduction or distribution, in whole or in part, is forbidden except by express written permission of Perun Software Ltd.
//

// https://en.cppreference.com/w/cpp/header/bit

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // you will get: access to lots of nonstandard GNU/Linux extension functions
#endif

#include "exceptions.hpp"
#include "logger.hpp"

#include <cstdint>
#include <string.h> // memmem
#include <unistd.h>

#include "binary_parser.hpp"
// #include "io/compiler_intrinsics.hpp"

#include <array>
#include <bit>

namespace networkinterface {

BufferMixin::~BufferMixin() {
  if (_ownning_storage) {
    free(_storage);
  }
}


std::shared_ptr<BufferMixin> BufferMixin::from_file_descripto(int fd, int mtu) {
  std::shared_ptr<BufferMixin> parser = std::make_shared<BufferMixin>();
  parser->_storage         = (uint8_t*)malloc(mtu);
  parser->_bytes_allocated = mtu;
  parser->_bytes_written   = read(fd, parser->_storage, mtu);
  parser->_ownning_storage = true;
  if (parser->_bytes_written < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK || ENOENT)
        {
            throw NoDataException("");
        }
        throw std::runtime_error("Error reading from TUN descriptor: " + std::to_string(errno));
  }
  Logger::getInstance().log("Read: " + std::to_string(parser->_bytes_written), Logger::Severity::INFO, "Parser");
  return parser;
}

BufferMixin BufferMixin::from_external_memory(uint8_t* external_pointer, int64_t allocated_size, int64_t data_size) {
  BufferMixin parser;
  parser._storage         = external_pointer;
  parser._bytes_allocated = allocated_size;
  parser._bytes_written   = data_size;
  return parser;
}

std::shared_ptr<BufferMixin> BufferMixin::clone() {
  std::shared_ptr<BufferMixin> clone = std::make_shared<BufferMixin>();
  clone->_storage         = (uint8_t*)malloc(_bytes_allocated);
  std::memcpy(clone->_storage, _storage, _bytes_written);
  clone->_bytes_allocated = _bytes_allocated;
  clone->_bytes_written   = _bytes_written;
  clone->_ownning_storage = true;
  clone->_position        = 0;
  return clone;
}

std::string BufferMixin::debug() const {
  std::stringstream ss;
  ss << "[B pos=" << _position << " l=" << _bytes_written << " c=" << _bytes_allocated << "]";
  return ss.str();
}

std::string BufferMixin::hex(int64_t pos, int64_t len) {
  if(pos < 0 || pos > this->len()) throw_with_trace(ArgumentError("pos out of bounds " + std::to_string(pos)));
  std::string s((char*)(_storage + pos), len);
  return string_to_hex(std::move(s));
}

std::string BufferMixin::todbgstr(int chars) const {
  if(_bytes_written == 0) {
    return "<binary-empty>";
  }
  if(_bytes_written > chars) {
    return "0x" + string_to_hex(std::string((char*)_storage, chars)) + "...Bx" + std::to_string(_bytes_written - chars);
  }
  return "0x" + string_to_hex(std::string((char*)_storage, _bytes_written));
}

void BufferMixin::seek(int64_t position) {
  if(position > _bytes_written) {
    throw_with_trace(BufferReadError(position - _bytes_written));
  }
  _position = position;
}

void BufferMixin::clear() {
  _bytes_written = 0;
  _position      = 0;
}

//
// Common read functions
//

int32_t ru8(BufferMixin& b) {
  auto pos = b._storage + b._position;
  b._position += 1;
  return pos[0];
}

int32_t ru16(BufferMixin& b) {
  auto pos = b._storage + b._position;
  b._position += 2;
  return (static_cast<uint32_t>(pos[0]) << 8) | (static_cast<uint32_t>(pos[1]));
}

int32_t rs16(BufferMixin& b) {
  constexpr int32_t size16 = 2;
  auto              pos    = b._storage + b._position;
  b._position += size16;
  auto x = (static_cast<int32_t>(pos[0]) << 8) | (static_cast<int32_t>(pos[1]));
  x |= -(x & (1L << ((8 * size16) - 1)));
  return x;
}

int32_t ru24(BufferMixin& b) {
  constexpr int32_t size24 = 3;
  auto              pos    = b._storage + b._position;
  b._position += size24;
  return (static_cast<uint32_t>(pos[0]) << 16) | (static_cast<uint32_t>(pos[1]) << 8) | (static_cast<uint32_t>(pos[2]));
}

int32_t rs24(BufferMixin& b) {
  constexpr int32_t size24 = 3;
  auto              pos    = b._storage + b._position;
  b._position += size24;
  auto x = (static_cast<int32_t>(pos[0]) << 16) | (static_cast<int32_t>(pos[1]) << 8) | (static_cast<int32_t>(pos[2]));
  x |= -(x & (1L << ((8 * size24) - 1)));
  return x;
}

int64_t ru32(BufferMixin& b) {
  constexpr int64_t size32 = 4;
  auto              pos    = b._storage + b._position;
  b._position += size32;
  return (static_cast<uint64_t>(pos[0]) << 24) | (static_cast<uint64_t>(pos[1]) << 16) | (static_cast<uint64_t>(pos[2]) << 8) | (static_cast<uint64_t>(pos[3]));
}

int64_t rs32(BufferMixin& b) {
  constexpr int64_t size32 = 4, one = 1;
  auto              pos = b._storage + b._position;
  b._position += size32;
  auto x = (static_cast<int64_t>(pos[0]) << 24) | (static_cast<int64_t>(pos[1]) << 16) | (static_cast<int64_t>(pos[2]) << 8) | (static_cast<int64_t>(pos[3]));
  x |= -(x & (one << ((8 * size32) - 1)));
  return x;
}

int64_t ru40(BufferMixin& b) {
  constexpr int64_t size40 = 5;
  auto              pos    = b._storage + b._position;
  b._position += size40;
  return (static_cast<uint64_t>(pos[0]) << 32) | (static_cast<uint64_t>(pos[1]) << 24) | (static_cast<uint64_t>(pos[2]) << 16) | (static_cast<uint64_t>(pos[3]) << 8) | (static_cast<uint64_t>(pos[4]));
}

int64_t rs40(BufferMixin& b) {
  constexpr int64_t size40 = 5, one = 1;
  auto              pos = b._storage + b._position;
  b._position += size40;
  auto x = (static_cast<int64_t>(pos[0]) << 32) | (static_cast<int64_t>(pos[1]) << 24) | (static_cast<int64_t>(pos[2]) << 16) | (static_cast<int64_t>(pos[3]) << 8) | (static_cast<int64_t>(pos[4]));
  x |= -(x & (one << ((8 * size40) - 1)));
  return x;
}

int64_t ru48(BufferMixin& b) {
  constexpr int64_t size48 = 6;
  auto              pos    = b._storage + b._position;
  b._position += size48;
  return (static_cast<uint64_t>(pos[0]) << 40) | (static_cast<uint64_t>(pos[1]) << 32) | (static_cast<uint64_t>(pos[2]) << 24) | (static_cast<uint64_t>(pos[3]) << 16) | (static_cast<uint64_t>(pos[4]) << 8) | (static_cast<uint64_t>(pos[5]));
}

int64_t rs48(BufferMixin& b) {
  constexpr int64_t size48 = 6, one = 1;
  auto              pos = b._storage + b._position;
  b._position += size48;
  auto x = (static_cast<int64_t>(pos[0]) << 40) | (static_cast<int64_t>(pos[1]) << 32) | (static_cast<int64_t>(pos[2]) << 24) | (static_cast<int64_t>(pos[3]) << 16) | (static_cast<int64_t>(pos[4]) << 8) | (static_cast<int64_t>(pos[5]));
  x |= -(x & (one << ((8 * size48) - 1)));
  return x;
}

int64_t ru56(BufferMixin& b) {
  constexpr int64_t size56 = 7;
  auto              pos    = b._storage + b._position;
  b._position += size56;
  return (static_cast<uint64_t>(pos[0]) << 48) | (static_cast<uint64_t>(pos[1]) << 40) | (static_cast<uint64_t>(pos[2]) << 32) | (static_cast<uint64_t>(pos[3]) << 24) | (static_cast<uint64_t>(pos[4]) << 16) | (static_cast<uint64_t>(pos[5]) << 8) | (static_cast<uint64_t>(pos[6]));
}

int64_t rs56(BufferMixin& b) {
  constexpr int64_t size56 = 7, one = 1;
  auto              pos = b._storage + b._position;
  b._position += size56;
  auto x = (static_cast<int64_t>(pos[0]) << 48) | (static_cast<int64_t>(pos[1]) << 40) | (static_cast<int64_t>(pos[2]) << 32) | (static_cast<int64_t>(pos[3]) << 24) | (static_cast<int64_t>(pos[4]) << 16) | (static_cast<int64_t>(pos[5]) << 8) | (static_cast<int64_t>(pos[6]));
  x |= -(x & (one << ((8 * size56) - 1)));
  return x;
}

int64_t rs64(BufferMixin& b) {
  constexpr int64_t size64 = 8;
  auto              pos    = b._storage + b._position;
  b._position += size64;
  int64_t value = (static_cast<int64_t>(pos[0]) << 48) | (static_cast<int64_t>(pos[1]) << 40) | (static_cast<int64_t>(pos[2]) << 32) | (static_cast<int64_t>(pos[3]) << 24) | (static_cast<int64_t>(pos[4]) << 16) | (static_cast<int64_t>(pos[5]) << 8) | static_cast<int64_t>(pos[6]);
  value         = (value << 8) | static_cast<int64_t>(pos[7]);
  return value;
}

int64_t ru64(BufferMixin& b) {
  constexpr int64_t size64 = 8;
  auto              pos    = b._storage + b._position;
  b._position += size64;
  return (static_cast<uint64_t>(pos[0]) << 56) | (static_cast<uint64_t>(pos[1]) << 48) | (static_cast<uint64_t>(pos[2]) << 40) | (static_cast<uint64_t>(pos[3]) << 32) | (static_cast<uint64_t>(pos[4]) << 24) | (static_cast<uint64_t>(pos[5]) << 16) | (static_cast<uint64_t>(pos[6]) << 8) | (static_cast<uint64_t>(pos[7]));

  // int64_t value;
  // value = ((int64_t) pos[7]);
  // value += ((int64_t) pos[6]) << 8;
  // value += ((int64_t) pos[5]) << 16;
  // value += ((int64_t) pos[4]) << 24;
  // value += ((int64_t) pos[3]) << 32;
  // value += ((int64_t) pos[2]) << 40;
  // value += ((int64_t) pos[1]) << 48;
  // value += ((int64_t) pos[1]) << 56;
  // return value;

  // this is optimized:
  // return *((int64_t*) pos);
}

uint8_t* rbytes(BufferMixin& b, int64_t size) {
  auto pos = b._position;
  b._position += size;
  return b._storage + pos;
}

uint8_t* rbytes_at_most(BufferMixin& b, int64_t max_size, int64_t& received) {
  if(b._position + max_size > b._bytes_written) max_size = b._bytes_written - b._position;
  auto pos = b._position;
  b._position += max_size;
  received = max_size;
  return b._storage + pos;
}

std::string rstring(BufferMixin& b, int64_t size) {
  auto pos = b._position;
  b._position += size;
  return std::string(reinterpret_cast<char*>(b._storage + pos), size);
}

// entire buffer read in case of no null terminator !
std::string rnullterminatedstring(BufferMixin& b) {
  auto found = memchr(reinterpret_cast<char*>(b.start_of_data()), '\0', b.bytes_left());
  if(found) {
    auto string_size = (uint8_t*)found - b.start_of_data();
    return b.rstring(string_size);
  } else {
    return b.rstring(b.bytes_left());
  }
}

double rdouble(BufferMixin& b) {
  uint64_t*      intp   = reinterpret_cast<uint64_t*>(b._storage + b._position);
  const uint64_t swaped = __builtin_bswap64(*intp);
  b._position += 8;
  return *(reinterpret_cast<const double*>(&swaped));
}

float rfloat(BufferMixin& b) {
  uint32_t*      intp   = reinterpret_cast<uint32_t*>(b._storage + b._position);
  const uint32_t swaped = __builtin_bswap32(*intp);
  b._position += 4;
  return *(reinterpret_cast<const float*>(&swaped));
}

float rfloat_le(BufferMixin& b) {
  float f = *(reinterpret_cast<float*>(b._storage + b._position));
  b._position += 4;
  return f;
}
//
// Common write functions:
//

void wu8(BufferMixin& b, int value) {
  auto pos = b._storage + b._position;
  pos[0]   = (char)(value & 0xFF);
  b._position += 1;
  b._bytes_written = std::max(b._position, b._bytes_written);
}
void wu16(BufferMixin& b, int value) {
  auto pos = b._storage + b._position;
  pos[0]   = (char)(value >> 8);
  pos[1]   = (char)(value);
  b._position += 2;
  b._bytes_written = std::max(b._position, b._bytes_written);
}
void wu16le(BufferMixin& b, int value) {
  auto pos = b._storage + b._position;
  pos[1]   = (char)(value >> 8);
  pos[0]   = (char)(value);
  b._position += 2;
  b._bytes_written = std::max(b._position, b._bytes_written);
}
void wu24(BufferMixin& b, int value) {
  auto pos = b._storage + b._position;
  pos[0]   = (char)(value >> 16);
  pos[1]   = (char)(value >> 8);
  pos[2]   = (char)(value);
  b._position += 3;
  b._bytes_written = std::max(b._position, b._bytes_written);
}
void wu32(BufferMixin& b, int value) {
  auto pos = b._storage + b._position;
  pos[0]   = (char)(value >> 24);
  pos[1]   = (char)(value >> 16);
  pos[2]   = (char)(value >> 8);
  pos[3]   = (char)(value);
  b._position += 4;
  b._bytes_written = std::max(b._position, b._bytes_written);
}
void wu32le(BufferMixin& b, int value) {
  auto pos = b._storage + b._position;
  pos[3]   = (char)(value >> 24);
  pos[2]   = (char)(value >> 16);
  pos[1]   = (char)(value >> 8);
  pos[0]   = (char)(value);
  b._position += 4;
  b._bytes_written = std::max(b._position, b._bytes_written);
}
void wu40(BufferMixin& b, int64_t value) {
  auto pos = b._storage + b._position;
  pos[0]   = (char)(value >> 32);
  pos[1]   = (char)(value >> 24);
  pos[2]   = (char)(value >> 16);
  pos[3]   = (char)(value >> 8);
  pos[4]   = (char)(value);
  b._position += 5;
  b._bytes_written = std::max(b._position, b._bytes_written);
}
void wu48(BufferMixin& b, int64_t value) {
  auto pos = b._storage + b._position;
  pos[0]   = (char)(value >> 40);
  pos[1]   = (char)(value >> 32);
  pos[2]   = (char)(value >> 24);
  pos[3]   = (char)(value >> 16);
  pos[4]   = (char)(value >> 8);
  pos[5]   = (char)(value);
  b._position += 6;
  b._bytes_written = std::max(b._position, b._bytes_written);
}
void wu56(BufferMixin& b, int64_t value) {
  auto pos = b._storage + b._position;
  pos[0]   = (char)(value >> 48);
  pos[1]   = (char)(value >> 40);
  pos[2]   = (char)(value >> 32);
  pos[3]   = (char)(value >> 24);
  pos[4]   = (char)(value >> 16);
  pos[5]   = (char)(value >> 8);
  pos[6]   = (char)(value);
  b._position += 7;
  b._bytes_written = std::max(b._position, b._bytes_written);
}
void wu64(BufferMixin& b, int64_t value) {
  auto pos = b._storage + b._position;
  //// ----------------
  pos[0]   = (char)(value >> 56);
  pos[1]   = (char)(value >> 48);
  pos[2]   = (char)(value >> 40);
  pos[3]   = (char)(value >> 32);
  pos[4]   = (char)(value >> 24);
  pos[5]   = (char)(value >> 16);
  pos[6]   = (char)(value >> 8);
  pos[7]   = (char)(value);
  // ^^^^^ when compiled with -O2 above 8 lines are 3 instructions:
  // mov     rdx, QWORD PTR b._storage[rip]
  // bswap   rdi
  // mov     QWORD PTR [rdx+rax], rdi
  //// ----------------
  b._position += 8;
  b._bytes_written = std::max(b._position, b._bytes_written);
}

void bor_byte(BufferMixin& b, int64_t pos, int mask) {
  b._storage[pos] |= mask;
}

void wbytes(BufferMixin& b, void* data, int64_t data_size) {
  std::memcpy(b._storage + b._position, data, data_size);
  b._position += data_size;
  b._bytes_written = std::max(b._position, b._bytes_written);
}

void wstring(BufferMixin& b, const std::string& text) {
  int64_t text_size = text.size();
  std::memcpy(b._storage + b._position, text.c_str(), text.size());
  b._position += text_size;
  b._bytes_written = std::max(b._position, b._bytes_written);
}

void wdouble(BufferMixin& b, double number) {
  void*     convert = &number;
  uint64_t* intp    = static_cast<uint64_t*>(convert);
  *intp             = __builtin_bswap64(*intp);
  uint64_t* pos     = reinterpret_cast<uint64_t*>(b._storage + b._position);
  *pos              = *intp;
  b._position += 8;
  b._bytes_written = std::max(b._position, b._bytes_written);
}

void wfloat(BufferMixin& b, float number) {
  void*     convert = &number;
  uint32_t* intp    = static_cast<uint32_t*>(convert);
  *intp             = __builtin_bswap32(*intp);
  uint32_t* pos     = reinterpret_cast<uint32_t*>(b._storage + b._position);
  *pos              = *intp;
  b._position += 4;
  b._bytes_written = std::max(b._position, b._bytes_written);
}

void wfloat_le(BufferMixin& b, float number) {
  float* const p = reinterpret_cast<float*>(b._storage + b._position);
  *p             = number;
  b._position += 4;
  b._bytes_written = std::max(b._position, b._bytes_written);
}

constexpr bool is_little_endian_machine() {
  // constexpr uint16_t number = 1;
  // auto               bytes  = std::bit_cast<std::array<uint8_t, 2>>(number);
  // return bytes[0] == 1;
  // // // constexpr int i=1;
  // // // return *((char *)&i);
  return std::endian::native == std::endian::little;
}

void BufferMixin::wfloat_le(float number) {
  // we rely on float being 4B, double 8B
  static_assert(sizeof(float) == 4, "float must be 4 bytes");
  static_assert(sizeof(double) == 8, "float must be 4 bytes");
  // we rely on little-endian machine
  static_assert(is_little_endian_machine(), "This machine is not little endian!");

  if(_position + 4 > _bytes_allocated) throw_with_trace(BufferWriteError(4 - _bytes_allocated + _position));
  networkinterface::wfloat_le(*this, number);
}

void wmkvuint(BufferMixin& b, int64_t number) {
  int64_t pos = b.tell();
  if(INT64_C(0x7F) > number) {
    b.wu8((int)number);
    b.bor_byte(pos, 0x80);
  } else if(INT64_C(0x3FFF) > number) {
    b.wu16((int)number);
    b.bor_byte(pos, 0x40);
  } else if(INT64_C(0x1FFFFF) > number) {
    b.wu24((int)number);
    b.bor_byte(pos, 0x20);
  } else if(INT64_C(0xFFFFFFF) > number) {
    b.wu32((int)number);
    b.bor_byte(pos, 0x10);
  } else if(INT64_C(0x07FFFFFFFF) > number) {
    b.wu40(number);
    b.bor_byte(pos, 0x08);
  } else if(INT64_C(0x03FFFFFFFFFF) > number) {
    b.wu48(number);
    b.bor_byte(pos, 0x04);
  } else if(INT64_C(0x01FFFFFFFFFFFF) > number) {
    b.wu56(number);
    b.bor_byte(pos, 0x02);
  } else {
    b.wu64(number);
    b.bor_byte(pos, 0x01);
  }
}

void BufferMixin::wu8(int value) {
  if(_position + 1 > _bytes_allocated) throw_with_trace(BufferWriteError(1));
  networkinterface::wu8(*this, value);
}
void BufferMixin::wu16(int value) {
  if(_position + 2 > _bytes_allocated) throw_with_trace(BufferWriteError(2 - _bytes_allocated + _position));
  networkinterface::wu16(*this, value);
}
void BufferMixin::wu16le(int value) {
  if(_position + 2 > _bytes_allocated) throw_with_trace(BufferWriteError(2 - _bytes_allocated + _position));
  networkinterface::wu16le(*this, value);
}
void BufferMixin::wu24(int value) {
  if(_position + 3 > _bytes_allocated) throw_with_trace(BufferWriteError(3 - _bytes_allocated + _position));
  networkinterface::wu24(*this, value);
}
void BufferMixin::wu32(int value) {
  if(_position + 4 > _bytes_allocated) throw_with_trace(BufferWriteError(4 - _bytes_allocated + _position));
  networkinterface::wu32(*this, value);
}
void BufferMixin::wu32le(int value) {
  if(_position + 4 > _bytes_allocated) throw_with_trace(BufferWriteError(4 - _bytes_allocated + _position));
  networkinterface::wu32le(*this, value);
}
void BufferMixin::wu40(int64_t value) {
  if(_position + 5 > _bytes_allocated) throw_with_trace(BufferWriteError(5 - _bytes_allocated + _position));
  networkinterface::wu40(*this, value);
}
void BufferMixin::wu48(int64_t value) {
  if(_position + 6 > _bytes_allocated) throw_with_trace(BufferWriteError(6 - _bytes_allocated + _position));
  networkinterface::wu48(*this, value);
}
void BufferMixin::wu56(int64_t value) {
  if(_position + 7 > _bytes_allocated) throw_with_trace(BufferWriteError(7 - _bytes_allocated + _position));
  networkinterface::wu56(*this, value);
}
void BufferMixin::wu64(int64_t value) {
  if(_position + 8 > _bytes_allocated) throw_with_trace(BufferWriteError(8 - _bytes_allocated + _position));
  networkinterface::wu64(*this, value);
}
void BufferMixin::wbytes(void* data, int64_t data_size) {
  if(_position + data_size > _bytes_allocated) throw_with_trace(BufferWriteError(data_size - _bytes_allocated + _position));
  networkinterface::wbytes(*this, data, data_size);
}
void BufferMixin::wstring(const std::string& text) {
  const int64_t text_size = text.size();
  if(_position + text_size > _bytes_allocated) throw_with_trace(BufferWriteError(text_size - _bytes_allocated + _position));
  networkinterface::wstring(*this, text);
}
void BufferMixin::wdouble(double number) {
  if(_position + 8 > _bytes_allocated) throw_with_trace(BufferWriteError(8 - _bytes_allocated + _position));
  networkinterface::wdouble(*this, number);
}
void BufferMixin::wfloat(float number) {
  if(_position + 4 > _bytes_allocated) throw_with_trace(BufferWriteError(4 - _bytes_allocated + _position));
  networkinterface::wfloat(*this, number);
}
void BufferMixin::wmkvuint(int64_t number) {
  if(_position + 8 > _bytes_allocated) throw_with_trace(BufferWriteError(8 - _bytes_allocated + _position));
  networkinterface::wmkvuint(*this, number);
}

void BufferMixin::bor_byte(int64_t pos, int mask) {
  if(pos > _bytes_allocated) throw_with_trace(BufferWriteError(pos - _bytes_allocated));
  _storage[pos] |= mask;
}

int32_t BufferMixin::ru8() {
  if(_position + 1 > _bytes_written) throw_with_trace(BufferReadError(1));
  return networkinterface::ru8(*this);
}

int32_t BufferMixin::ru16() {
  if(_position + 2 > _bytes_written) throw_with_trace(BufferReadError(2 - _bytes_written + _position));
  return networkinterface::ru16(*this);
}

int32_t BufferMixin::rs16() {
  constexpr int32_t size16 = 2;
  if(_position + size16 > _bytes_written) throw_with_trace(BufferReadError(size16 - _bytes_written + _position));
  return networkinterface::rs16(*this);
}

int32_t BufferMixin::ru24() {
  constexpr int32_t size24 = 3;
  if(_position + size24 > _bytes_written) throw_with_trace(BufferReadError(size24 - _bytes_written + _position));
  return networkinterface::ru24(*this);
}

int32_t BufferMixin::rs24() {
  constexpr int32_t size24 = 3;
  if(_position + size24 > _bytes_written) throw_with_trace(BufferReadError(size24 - _bytes_written + _position));
  return networkinterface::rs24(*this);
}

int64_t BufferMixin::ru32() {
  constexpr int64_t size32 = 4;
  if(_position + size32 > _bytes_written) throw_with_trace(BufferReadError(size32 - _bytes_written + _position));
  return networkinterface::ru32(*this);
}

int64_t BufferMixin::rs32() {
  constexpr int64_t size32 = 4, one = 1;
  if(_position + size32 > _bytes_written) throw_with_trace(BufferReadError(size32 - _bytes_written + _position));
  return networkinterface::rs32(*this);
}

int64_t BufferMixin::ru40() {
  constexpr int64_t size40 = 5;
  if(_position + size40 > _bytes_written) throw_with_trace(BufferReadError(size40 - _bytes_written + _position));
  return networkinterface::ru40(*this);
}

int64_t BufferMixin::rs40() {
  constexpr int64_t size40 = 5, one = 1;
  if(_position + size40 > _bytes_written) throw_with_trace(BufferReadError(size40 - _bytes_written + _position));
  return networkinterface::rs40(*this);
}

int64_t BufferMixin::ru48() {
  constexpr int64_t size48 = 6;
  if(_position + size48 > _bytes_written) throw_with_trace(BufferReadError(size48 - _bytes_written + _position));
  return networkinterface::ru48(*this);
}

int64_t BufferMixin::rs48() {
  constexpr int64_t size48 = 6, one = 1;
  if(_position + size48 > _bytes_written) throw_with_trace(BufferReadError(size48 - _bytes_written + _position));
  return networkinterface::rs48(*this);
}

int64_t BufferMixin::ru56() {
  constexpr int64_t size56 = 7;
  if(_position + size56 > _bytes_written) throw_with_trace(BufferReadError(size56 - _bytes_written + _position));
  return networkinterface::ru56(*this);
}

int64_t BufferMixin::rs56() {
  constexpr int64_t size56 = 7, one = 1;
  if(_position + size56 > _bytes_written) throw_with_trace(BufferReadError(size56 - _bytes_written + _position));
  return networkinterface::rs56(*this);
}

int64_t BufferMixin::rs64() {
  constexpr int64_t size64 = 8;
  if(_position + size64 > _bytes_written) throw_with_trace(BufferReadError(size64 - _bytes_written + _position));
  return networkinterface::rs64(*this);
}

int64_t BufferMixin::ru64() {
  constexpr int64_t size64 = 8;
  if(_position + size64 > _bytes_written) throw_with_trace(BufferReadError(size64 - _bytes_written + _position));
  return networkinterface::ru64(*this);
}

extern const uint8_t byte_to_size_table[256];

const uint8_t byte_to_size_table[256] = {
  8, 8, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

const uint64_t bytecount_to_negative_mask[8] = {
  UINT64_C(0x40),
  UINT64_C(0x2000),
  UINT64_C(0x100000),
  UINT64_C(0x8000000),
  UINT64_C(0x400000000),
  UINT64_C(0x20000000000),
  UINT64_C(0x1000000000000),
  UINT64_C(0x80000000000000),
};
const uint64_t bytecount_to_signed_mask[8] = {
  UINT64_C(0xFFFFFFFFFFFFFF80),
  UINT64_C(0xFFFFFFFFFFFFC000),
  UINT64_C(0xFFFFFFFFFFE00000),
  UINT64_C(0xFFFFFFFFF0000000),
  UINT64_C(0xFFFFFFF800000000),
  UINT64_C(0xFFFFFC0000000000),
  UINT64_C(0xFFFE000000000000),
  UINT64_C(0xFF00000000000000),
};
const uint64_t bytecount_to_minusone[8] = {
  UINT64_C(0x7F),
  UINT64_C(0x3FFF),
  UINT64_C(0x1FFFFF),
  UINT64_C(0x0FFFFFFF),
  UINT64_C(0x07FFFFFFFF),
  UINT64_C(0x03FFFFFFFFFF),
  UINT64_C(0x01FFFFFFFFFFFF),
  UINT64_C(0x00FFFFFFFFFFFFFF),
};

// Done: use strnlen_s
// // returns the number of characters that precede the terminating null character.
// size_t bounded_strlen(const char* start, size_t size) {
//   size_t index;
//   for(index = 0;; index++) {
//     if(index >= size) {
//       throw_with_trace(BufferReadError(index));
//     }
//     if(start[index] == '\0') return index;
//   }
// }

void BufferMixin::rbytes_into(uint8_t* destination, int64_t size) {
  if(_position + size > _bytes_written) throw_with_trace(BufferReadError(_bytes_written - (_position + size)));
  memcpy(destination, _storage + _position, size);
  _position += size;
}

uint8_t* BufferMixin::rbytes(int64_t size) {
  if(_position + size > _bytes_written) throw_with_trace(BufferReadError(_bytes_written - (_position + size)));
  return networkinterface::rbytes(*this, size);
}

uint8_t* BufferMixin::rbytes_at_most(int64_t max_size, int64_t& received) {
  if(_position + max_size > _bytes_written) max_size = _bytes_written - _position;
  return networkinterface::rbytes_at_most(*this, max_size, received);
}

std::string BufferMixin::rstring(int64_t size) {
  if(_position + size > _bytes_written) throw_with_trace(BufferReadError(_bytes_written - (_position + size)));
  return networkinterface::rstring(*this, size);
}

std::string BufferMixin::rnullterminatedstring() {
  return networkinterface::rnullterminatedstring(*this);
}

double BufferMixin::rdouble() {
  if(_position + 8 > _bytes_written) throw_with_trace(BufferReadError(_bytes_written - _position - 8));
  return networkinterface::rdouble(*this);
}

float BufferMixin::rfloat() {
  if(_position + 4 > _bytes_written) throw_with_trace(BufferReadError(_bytes_written - _position - 4));
  return networkinterface::rfloat(*this);
}

float BufferMixin::rfloat_le() {
  if(_position + 4 > _bytes_written) throw_with_trace(BufferReadError(_bytes_written - _position - 4));
  return networkinterface::rfloat_le(*this);
}

// // DONE: Fix this legacy code:
// void* custom_memmem(const void* haystack, size_t haystack_len, const void* const needle, const size_t needle_len) {
//   if(needle_len == 0) return (void*)haystack; // The first occurrence of the empty string is deemed to occur at the beginning of the string
//   if(haystack == NULL) return NULL;
//   if(haystack_len == 0) return NULL;
//   if(needle == NULL) return NULL;
//   //  for(const char *h = haystack; haystack_len >= needle_len; ++h, --haystack_len) {
//   //    if(!memcmp(h, needle, needle_len)) {
//   //      return h;
//   //    }
//   //  }
//   //  return NULL;
//   const char*       begin;
//   const char* const last_possible = (const char*)haystack + haystack_len - needle_len;
//   for(begin = (const char*)haystack; begin <= last_possible; ++begin)
//     if(begin[0] == ((const char*)needle)[0] && !memcmp((const void*)&begin[1], (const void*)((const char*)needle + 1), needle_len - 1)) return (void*)begin;
//   return NULL;
// }

int64_t BufferMixin::find_binary(const char* needle, int64_t needle_size) const {
  char* start = reinterpret_cast<char*>(start_of_data());
  char* found = reinterpret_cast<char*>(::memmem(start, bytes_left(), needle, needle_size));
  if(found == nullptr) return -1;
  int64_t distance = (int64_t)(found - start);
  return distance;
}

void BufferMixin::consume_bytes(int64_t amount) {
  _position += amount;
  if(_position > _bytes_allocated) {
    throw_with_trace(BufferWriteError(_position - _bytes_allocated));
  }
  _bytes_written = std::max(_bytes_written, _position);
}

void BufferMixin::bytes_appended(int64_t amount) {
  _bytes_written += amount;
  if(_bytes_written > _bytes_allocated) {
    //    l.e("MemoryBuffer", "bytes_appended BufferOverflow", { { "_bytes_written", _bytes_written }, { "_bytes_allocated", _bytes_allocated }, { "amount", amount } });
    throw_with_trace(BufferWriteError(_bytes_written - _bytes_allocated));
  }
}

//
// WriteInterface
//

void WriteInterface::wb(const BufferMixin& source) {
  if(source.len() == 0) throw_with_trace(WriteError("wb on empty MemoryBuffer"));
  w(source._storage, source.len());
}

void WriteInterface::wb(BufferMixin& source, int64_t bytes_to_write) {
  auto amount = std::min(bytes_to_write, source.bytes_left());
  if(amount == 0) throw_with_trace(WriteError("wb missing " + std::to_string(bytes_to_write)));
  w(source.start_of_data(), amount);
  source.consume_bytes(amount);
}

void WriteInterface::wstring(const std::string& bytes) {
  w((uint8_t*)bytes.c_str(), bytes.size());
}

DecodeError::DecodeError(std::string what)
    : ErrorBase(std::move(what)) { }

std::string string_to_hex(const std::string& input) {
  static const char hex_digits[] = "0123456789ABCDEF";

  std::string output;
  output.reserve(input.length() * 2);
  for(unsigned char c: input) {
    output.push_back(hex_digits[c >> 4]);
    output.push_back(hex_digits[c & 15]);
  }
  return output;
}

MemoryBuffer::MemoryBuffer() {
  _bytes_allocated = 0;
  _storage         = nullptr;
  _position        = 0;
  _bytes_written   = 0;
  _CHUNK_SIZE_     = 8192;
}
MemoryBuffer::MemoryBuffer(int64_t initial_size) {
  _bytes_allocated = initial_size;
  _storage         = (uint8_t*)malloc(initial_size);
  _position        = 0;
  _bytes_written   = 0;
  _CHUNK_SIZE_     = 8192;
}
MemoryBuffer::MemoryBuffer(void* data, int64_t data_size) {
  _bytes_allocated = data_size;
  _storage         = (uint8_t*)malloc(_bytes_allocated);
  _position        = 0;
  _bytes_written   = data_size;
  _CHUNK_SIZE_     = 8192;
  memcpy(_storage, data, data_size);
}

MemoryBuffer::MemoryBuffer(const MemoryBuffer& original) {
  _bytes_allocated = original._bytes_written;
  _storage         = (uint8_t*)malloc(_bytes_allocated);
  memcpy(_storage, original._storage, original._bytes_written);
  _position      = original._position;
  _bytes_written = original._bytes_written;
  _CHUNK_SIZE_   = original._CHUNK_SIZE_;
}
MemoryBuffer& MemoryBuffer::operator=(const MemoryBuffer& other) {
  if(this != &other) {
    free(_storage);
    _bytes_allocated = other._bytes_written;
    _storage         = (uint8_t*)malloc(other._bytes_written);
    memcpy(_storage, other._storage, other._bytes_written);
    _position      = other._position;
    _bytes_written = other._bytes_written;
    _CHUNK_SIZE_   = other._CHUNK_SIZE_;
  }
  return *this;
}

MemoryBuffer::MemoryBuffer(MemoryBuffer&& other) {
  _bytes_allocated       = other._bytes_allocated;
  _bytes_written         = other._bytes_written;
  _position              = other._position;
  _storage               = other._storage;
  _CHUNK_SIZE_           = other._CHUNK_SIZE_;
  other._position        = 0;
  other._bytes_allocated = 0;
  other._bytes_written   = 0;
  other._storage         = nullptr;
}
MemoryBuffer& MemoryBuffer::operator=(MemoryBuffer&& other) {
  if(this != &other) {
    free(_storage);
    _bytes_allocated       = other._bytes_allocated;
    _bytes_written         = other._bytes_written;
    _position              = other._position;
    _storage               = other._storage;
    _CHUNK_SIZE_           = other._CHUNK_SIZE_;
    other._position        = 0;
    other._bytes_allocated = 0;
    other._bytes_written   = 0;
    other._storage         = nullptr;
  }
  return *this;
}

MemoryBuffer::~MemoryBuffer() {
  free(_storage);
}

void MemoryBuffer::eliminate_parsed_data(bool forced) {
  if(_position == _bytes_written) {
    clear();
    return;
  }
  // TODO: replace hardcoded factor 8
  if(forced || _position > 8 * bytes_left()) {
    // move usefull data:
    memmove(_storage, start_of_data(), bytes_left());
    _bytes_written = _bytes_written - _position;
    _position      = 0;
  }
}

// Using this method is always a bug !
// void MemoryBuffer::reserve_capacity_from_start(int64_t amount_needed) {
//   int64_t existing_space = _bytes_allocated - _position;
//   if(existing_space < amount_needed) {
//     int64_t space_to_allocate = _position + amount_needed + (_CHUNK_SIZE_ * 4); // add extra space to allocation to reduce number of realloc()'s
//     _storage                  = (uint8_t*)realloc(_storage, space_to_allocate);
//     if(_storage == NULL) {
//       // LOGE << "MemoryBuffer::reserve_capacity_from_start realloc() error amount_needed=" << amount_needed << " space_to_allocate=" << space_to_allocate;
//       throw_with_trace(ErrorBase("MemoryBuffer::realloc() start"));
//     }
//     _bytes_allocated = space_to_allocate;
//   }
// }

void MemoryBuffer::reserve_capacity_from_end(int64_t amount_needed) {
  if(_bytes_allocated < amount_needed + _bytes_written) {
    int64_t space_to_allocate = amount_needed + _bytes_written + (_CHUNK_SIZE_ * 1); // add extra space to allocation to reduce number of realloc()'s
    _storage                  = (uint8_t*)realloc(_storage, space_to_allocate);
    if(_storage == NULL) {
      // LOGE << "MemoryBuffer::reserve_capacity_from_end realloc() error amount_needed=" << amount_needed << " space_to_allocate=" << space_to_allocate;
      throw_with_trace(ErrorBase("MemoryBuffer::realloc()"));
    }
    _bytes_allocated = space_to_allocate;
  }
}

void MemoryBuffer::reserve_capacity_from_position(int64_t amount_needed) {
  if(_bytes_allocated < amount_needed + _position) {
    int64_t space_to_allocate = amount_needed + _position + (_CHUNK_SIZE_ * 1); // add extra space to allocation to reduce number of realloc()'s
    _storage                  = (uint8_t*)realloc(_storage, space_to_allocate);
    if(_storage == NULL) {
      throw_with_trace(ErrorBase("MemoryBuffer::realloc() 2"));
    }
    _bytes_allocated = space_to_allocate;
  }
}

void MemoryBuffer::write_to_end(void* data, int64_t amount) {
  reserve_capacity_from_end(amount);
  memcpy(end_of_data(), data, amount);
  _bytes_written += amount;
}

void MemoryBuffer::load_payload(ReadInterface& input, int64_t size) {
  clear();
  reserve_capacity_from_position(size);
  input.rbytes_into(_storage, size);
  _bytes_written = size;
}

void MemoryBuffer::wu8(int value) {
  reserve_capacity_from_position(1);
  networkinterface::wu8(*this, value);
}
void MemoryBuffer::wu16(int value) {
  reserve_capacity_from_position(2);
  networkinterface::wu16(*this, value);
}
void MemoryBuffer::wu16le(int value) {
  reserve_capacity_from_position(2);
  networkinterface::wu16le(*this, value);
}
void MemoryBuffer::wu24(int value) {
  reserve_capacity_from_position(3);
  networkinterface::wu24(*this, value);
}
void MemoryBuffer::wu32(int value) {
  reserve_capacity_from_position(4);
  networkinterface::wu32(*this, value);
}
void MemoryBuffer::wu32le(int value) {
  reserve_capacity_from_position(4);
  networkinterface::wu32le(*this, value);
}
void MemoryBuffer::wu40(int64_t value) {
  reserve_capacity_from_position(5);
  networkinterface::wu40(*this, value);
}
void MemoryBuffer::wu48(int64_t value) {
  reserve_capacity_from_position(6);
  networkinterface::wu48(*this, value);
}
void MemoryBuffer::wu56(int64_t value) {
  reserve_capacity_from_position(7);
  networkinterface::wu56(*this, value);
}
void MemoryBuffer::wu64(int64_t value) {
  reserve_capacity_from_position(8);
  networkinterface::wu64(*this, value);
}
void MemoryBuffer::wbytes(void* data, int64_t data_size) {
  reserve_capacity_from_position(data_size);
  networkinterface::wbytes(*this, data, data_size);
}
void MemoryBuffer::wstring(const std::string& text) {
  const int64_t text_size = text.size();
  reserve_capacity_from_position(text_size);
  networkinterface::wstring(*this, text);
}
void MemoryBuffer::wdouble(double number) {
  reserve_capacity_from_position(8);
  networkinterface::wdouble(*this, number);
}
void MemoryBuffer::wfloat(float number) {
  reserve_capacity_from_position(4);
  networkinterface::wfloat(*this, number);
}

void MemoryBuffer::wfloat_le(float number) {
  reserve_capacity_from_position(4);
  networkinterface::wfloat_le(*this, number);
}

void append_buffer(MemoryBuffer& dest, MemoryBuffer& src) {
  auto pos = dest.tell();
  dest.seek(dest.len());
  dest.wbytes(src._storage, src.len());
  dest.seek(pos);
}


} // namespace Perun
