#pragma once

#include <vector>

#include "file_descriptor.hh"
#include "mmap.hh"
#include "spans.hh"

class RingStorage
{
  FileDescriptor fd_;
  MMap_Region virtual_address_space_, first_mapping_, second_mapping_;

protected:
  string_span mutable_storage( const size_t index )
  {
    return { virtual_address_space_.addr() + index, capacity() };
  }

  std::string_view storage( const size_t index ) const
  {
    return { virtual_address_space_.addr() + index, capacity() };
  }

public:
  explicit RingStorage( const size_t capacity );

  size_t capacity() const { return first_mapping_.length(); }
};

class RingBuffer : public RingStorage
{
  size_t bytes_pushed_ = 0, bytes_popped_ = 0;

  size_t next_index_to_write() const;
  size_t next_index_to_read() const;

public:
  using RingStorage::RingStorage;

  string_span writable_region();
  std::string_view writable_region() const;
  void push( const size_t num_bytes );

  std::string_view readable_region() const;
  void pop( const size_t num_bytes );

  void push_from_fd( FileDescriptor& fd ) { push( fd.read( writable_region() ) ); }
  void pop_to_fd( FileDescriptor& fd ) { pop( fd.write( readable_region() ) ); }

  size_t push_from_const_str( const std::string_view str );
  void read_from_str( std::string_view& str ) { str.remove_prefix( push_from_const_str( str ) ); }

  size_t bytes_pushed() const { return bytes_pushed_; }
  size_t bytes_popped() const { return bytes_popped_; }
  size_t bytes_stored() const { return bytes_pushed_ - bytes_popped_; }

  bool can_write() const { return not writable_region().empty(); }
  bool can_read() const { return not readable_region().empty(); }
};
