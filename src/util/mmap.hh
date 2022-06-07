#pragma once

#include <cstdlib>
#include <string_view>

#include "file_descriptor.hh"

class MMap_Region
{
  char* addr_;
  size_t length_;

public:
  MMap_Region( char* const addr,
               const size_t length,
               const int prot,
               const int flags,
               const int fd,
               const off_t offset = 0 );

  ~MMap_Region();

  /* Disallow copying */
  MMap_Region( const MMap_Region& other ) = delete;
  MMap_Region& operator=( const MMap_Region& other ) = delete;

  /* Allow move-construction and move-assignment */
  MMap_Region( MMap_Region&& other ) noexcept;
  MMap_Region& operator=( MMap_Region&& other ) noexcept;

  char* addr() const { return addr_; }
  size_t length() const { return length_; }

  operator std::string_view() const { return { addr(), length() }; }
};

class ReadOnlyFile : public MMap_Region
{
  FileDescriptor fd_;

public:
  ReadOnlyFile( FileDescriptor&& fd );
  ReadOnlyFile( const std::string& filename );
};

class ReadWriteFile : public MMap_Region
{
  FileDescriptor fd_;

public:
  ReadWriteFile( FileDescriptor&& fd );
  operator string_span() const { return { addr(), length() }; }
};
