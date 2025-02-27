//
// Copyright 2021 (c) Perun Software Ltd.
// This unpublished material is proprietary to Perun Software Ltd. All rights reserved.
// The methods and techniques described herein are considered trade secrets and/or confidential.
// Reproduction or distribution, in whole or in part, is forbidden except by express written permission of Perun Software Ltd.
//

#ifndef NETWORK_MEYHAM_DATABUFFER_HPP_
#define NETWORK_MEYHAM_DATABUFFER_HPP_

#include "exceptions.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace networkinterface {

std::string string_to_hex(const std::string& input);

class DecodeError : public ErrorBase {
public:
  DecodeError(std::string what);
};

//
// When reading data we can use `ReadInterface` to parse primitives from [socket or file] input stream.
// When writting to file or socket we use `MemoryBuffer` and send prepared binary to the stream.
//
// Used as base class (interface) for BufferMixin, MemoryBuffer
//
class ReadInterface {
public:
  //
  // makes return pointer from `rbytes()` invalid !
  virtual void eliminate_parsed_data(bool forced=false) = 0;

  // Read exact `size` into cache and return pointer to stored data. Next read will invalidate returned pointer if realloc happens.
  virtual uint8_t* rbytes(int64_t size)                                = 0;
  // Read up to `max_size` bytes into cache and return pointer to stored data, `received` shall contain exact byte count available on returned pointer. `received == 0` means end of stream.
  virtual uint8_t* rbytes_at_most(int64_t max_size, int64_t& received) = 0;
  // Read exact `size` into given `destination`, skipping cache. Existing data from the cache is first consumed and then read follows.
  virtual void     rbytes_into(uint8_t* destination, int64_t size)     = 0;

  virtual int32_t     ru8()                 = 0;
  virtual int32_t     ru16()                = 0;
  virtual int32_t     rs16()                = 0;
  virtual int32_t     ru24()                = 0;
  virtual int32_t     rs24()                = 0;
  virtual int64_t     ru32()                = 0;
  virtual int64_t     rs32()                = 0;
  virtual int64_t     ru40()                = 0;
  virtual int64_t     rs40()                = 0;
  virtual int64_t     ru48()                = 0;
  virtual int64_t     rs48()                = 0;
  virtual int64_t     ru56()                = 0;
  virtual int64_t     rs56()                = 0;
  virtual int64_t     ru64()                = 0;
  virtual int64_t     rs64()                = 0;
  virtual std::string rstring(int64_t size) = 0;
  virtual double      rdouble()             = 0;
  virtual float       rfloat()              = 0;
  virtual float       rfloat_le()           = 0;

protected:
  ~ReadInterface() = default;
};

// Operates on given, static memory. Caller owns the memory.
// If overflow happens exception is raised.
class BufferMixin : public ReadInterface {
public:
  uint8_t* _storage         = nullptr;
  int64_t  _bytes_allocated = 0;
  int64_t  _bytes_written   = 0;
  int64_t  _position        = 0;
  bool     _ownning_storage = false;

  // Does not free memory given in constructor.
  ~BufferMixin();

  // does nothing
  void               eliminate_parsed_data(bool forced=false) override {};
  static std::shared_ptr<BufferMixin> from_file_descripto(int fd, int mtu);
  static BufferMixin from_external_memory(uint8_t* external_pointer, int64_t allocated_size, int64_t data_size);
  std::shared_ptr<BufferMixin> clone();

  std::string hex(int64_t pos, int64_t len);
  std::string debug() const;
  std::string todbgstr(int chars = 20) const;


public:
  // pointer for read operation to send what is left in buffer (POSITION).
  uint8_t* start_of_data() const { return _storage + _position; }
  // pointer for write operation to append to the end (BYTES-WRITTEN).
  uint8_t* end_of_data() const { return _storage + _bytes_written; }
  // bytes left to read
  int64_t  bytes_left() const { return _bytes_written - _position; }
  int64_t  capacity_left() const { return _bytes_allocated - _bytes_written; }
  // total size of useful data
  int64_t  len() const { return _bytes_written; }
  int64_t  tell() const { return _position; }
  void     seek(int64_t position);
  void     clear();

  // in write operation some bytes were consumed, move `_position` by given amount
  void consume_bytes(int64_t amount);
  // in read operation some bytes were written past `len()`, expand `_bytes_written` by given amount
  void bytes_appended(int64_t amount);

public:
  // Write methods can allocate more space in derived `MemoryBuffer`
  virtual void wu8(int value);
  virtual void wu16(int value);
  virtual void wu16le(int value);
  virtual void wu24(int value);
  virtual void wu32(int value);
  virtual void wu32le(int value);
  virtual void wu40(int64_t value);
  virtual void wu48(int64_t value);
  virtual void wu56(int64_t value);
  virtual void wu64(int64_t value);
  virtual void wbytes(void* data, int64_t data_size);
  virtual void wstring(const std::string& text);
  virtual void wdouble(double number);
  virtual void wfloat(float number);
  virtual void wfloat_le(float number);
  virtual void wmkvuint(int64_t number);

  void bor_byte(int64_t pos, int mask);

public:
  // Read methods throw exception on buffer overflow
  int32_t     ru8() override;
  int32_t     ru16() override;
  int32_t     rs16() override;
  int32_t     ru24() override;
  int32_t     rs24() override;
  int64_t     ru32() override;
  int64_t     rs32() override;
  int64_t     ru40() override;
  int64_t     rs40() override;
  int64_t     ru48() override;
  int64_t     rs48() override;
  int64_t     ru56() override;
  int64_t     rs56() override;
  int64_t     ru64() override;
  int64_t     rs64() override;
  uint8_t*    rbytes(int64_t size) override;
  uint8_t*    rbytes_at_most(int64_t max_size, int64_t& received) override;
  void        rbytes_into(uint8_t* destination, int64_t size) override;
  std::string rstring(int64_t size) override;
  std::string rnullterminatedstring();
  double      rdouble() override;
  float       rfloat() override;
  float       rfloat_le() override;

  int64_t find_binary(const char* needle, int64_t needle_size) const;
};

//
// Common read functions:
//

int32_t     ru8(BufferMixin& b);
int32_t     ru16(BufferMixin& b);
int32_t     rs16(BufferMixin& b);
int32_t     ru24(BufferMixin& b);
int32_t     rs24(BufferMixin& b);
int64_t     ru32(BufferMixin& b);
int64_t     rs32(BufferMixin& b);
int64_t     ru40(BufferMixin& b);
int64_t     rs40(BufferMixin& b);
int64_t     ru48(BufferMixin& b);
int64_t     rs48(BufferMixin& b);
int64_t     ru56(BufferMixin& b);
int64_t     rs56(BufferMixin& b);
int64_t     ru64(BufferMixin& b);
int64_t     rs64(BufferMixin& b);
uint8_t*    rbytes(BufferMixin& b, int64_t size);
uint8_t*    rbytes_at_most(BufferMixin& b, int64_t max_size, int64_t& received);
// void        rbytes_into(BufferMixin& b, uint8_t* destination, int64_t size);
std::string rstring(BufferMixin& b, int64_t size);
std::string rnullterminatedstring(BufferMixin& b);
double      rdouble(BufferMixin& b);
float       rfloat(BufferMixin& b);
float       rfloat_le(BufferMixin& b);

//
// Common write functions:
//

void wu8(BufferMixin& b, int value);
void wu16(BufferMixin& b, int value);
void wu16le(BufferMixin& b, int value);
void wu24(BufferMixin& b, int value);
void wu32(BufferMixin& b, int value);
void wu32le(BufferMixin& b, int value);
void wu40(BufferMixin& b, int64_t value);
void wu48(BufferMixin& b, int64_t value);
void wu56(BufferMixin& b, int64_t value);
void wu64(BufferMixin& b, int64_t value);
void bor_byte(BufferMixin& b, int64_t pos, int mask);
void wbytes(BufferMixin& b, void* data, int64_t data_size);
void wstring(BufferMixin& b, const std::string& text);
void wdouble(BufferMixin& b, double number);
void wfloat(BufferMixin& b, float number);
void wfloat_le(BufferMixin& b, float number);
void wmkvuint(BufferMixin& b, int64_t number);

// Allocates needed memory for read & write operations.
// If overflow happens additional memory is allocated.
class MemoryBuffer : public BufferMixin {
public:
  MemoryBuffer();
  explicit MemoryBuffer(int64_t initial_size);
  MemoryBuffer(void* data_to_cpy, int64_t data_size);
  MemoryBuffer(const MemoryBuffer& original);
  MemoryBuffer& operator=(const MemoryBuffer& other);
  MemoryBuffer(MemoryBuffer&& other);
  MemoryBuffer& operator=(MemoryBuffer&& other);
  virtual ~MemoryBuffer();

  // renders all prior data pointers invalid !
  void eliminate_parsed_data(bool forced=false) override;

  // reserve - for appending new data
  // // // void reserve_capacity_from_start(int64_t amount_needed); <-- this is always memory leak
  void reserve_capacity_from_end(int64_t amount_needed);
  void reserve_capacity_from_position(int64_t amount_needed);
  void write_to_end(void* data, int64_t amount);

  void load_payload(ReadInterface& input, int64_t size);

public:
  // possible realloc() to extend allocated space
  void wu8(int value) override;
  void wu16(int value) override;
  void wu16le(int value) override;
  void wu24(int value) override;
  void wu32(int value) override;
  void wu32le(int value) override;
  void wu40(int64_t value) override;
  void wu48(int64_t value) override;
  void wu56(int64_t value) override;
  void wu64(int64_t value) override;
  void wbytes(void* data, int64_t data_size) override;
  void wstring(const std::string& text) override;
  void wdouble(double number) override;
  void wfloat(float number) override;
  void wfloat_le(float number) override;

private:
  int _CHUNK_SIZE_;
};

void append_buffer(MemoryBuffer& dest, MemoryBuffer& src);

// ReadInterface + MemoryBuffer
class ReaderCache : public ReadInterface {
public:
  using U = std::unique_ptr<ReaderCache>;

  explicit ReaderCache(MemoryBuffer& buffer_to_use);
  virtual ~ReaderCache() = default;

  // Forbid empyt, copy & move
  ReaderCache()                    = delete;
  ReaderCache(ReaderCache& other)  = delete;
  ReaderCache(ReaderCache&& other) = delete;

  virtual void read_at_least(uint8_t* destination, int64_t& destination_position, int64_t min_size, int64_t max_size) = 0;
  //
  // makes return pointer from `rbytes()` invalid !
  void         eliminate_parsed_data(bool forced=false) override;

  uint8_t*    rbytes(int64_t size) override;
  uint8_t*    rbytes_at_most(int64_t max_size, int64_t& received) override;
  void        rbytes_into(uint8_t* destination, int64_t size) override;
  int32_t     ru8() override;
  int32_t     ru16() override;
  int32_t     rs16() override;
  int32_t     ru24() override;
  int32_t     rs24() override;
  int64_t     ru32() override;
  int64_t     rs32() override;
  int64_t     ru40() override;
  int64_t     rs40() override;
  int64_t     ru48() override;
  int64_t     rs48() override;
  int64_t     ru56() override;
  int64_t     rs56() override;
  int64_t     ru64() override;
  int64_t     rs64() override;
  std::string rstring(int64_t size) override;
  double      rdouble() override;
  float       rfloat() override;
  float       rfloat_le() override;

  // Exposed:
  MemoryBuffer& _buffer;

protected:
  static constexpr int64_t READ_CHUNKSIZE = 16384;
  void                     _read_into_buffer(int64_t size, int64_t maxsize = READ_CHUNKSIZE);
};

// When writting to file or socket we use `MemoryBuffer` and send prepared binary to the stream.
// `MemoryBuffer` lives outside I/O primitive subclassing WriteInterface
class WriteInterface {
public:
  using U                                                 = std::unique_ptr<WriteInterface>;
  // Send memory chunk
  virtual void w(uint8_t* source, int64_t bytes_to_write) = 0;

  // Send entire buffer, including consumed part
  void wb(const BufferMixin& source);
  // Send valid portion of the buffer, consuming it in the process
  void wb(BufferMixin& source, int64_t bytes_to_write);

  void wstring(const std::string& bytes);
};

// Socket, File, etc implement this interface
class IOStream : public ReaderCache, public WriteInterface {
public:
  using U = std::unique_ptr<IOStream>;
  explicit IOStream(MemoryBuffer& buffer_to_use)
      : ReaderCache(buffer_to_use) {};
};

class FileInterface : public IOStream {
public:
  explicit FileInterface(MemoryBuffer& buffer_to_use)
      : IOStream(buffer_to_use) {};
  virtual void    flush()           = 0;
  virtual int64_t tell()            = 0;
  virtual void    seek(int64_t pos) = 0;
};

} // namespace Perun

#endif // NETWORK_MEYHAM_DATABUFFER_HPP_
