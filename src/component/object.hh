#pragma once

#include <fcntl.h>
#include <sys/mman.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <variant>

#include <glog/logging.h>

#include "exception.hh"
#include "handle.hh"
#include "spans.hh"
#include "thunk.hh"

using Blob = std::span<const char>;
using Tree = std::span<const Handle>;
using Object = std::variant<Blob, Tree>;

template<typename T>
constexpr inline bool is_object = false;

template<>
constexpr inline bool is_object<Blob> = true;

template<>
constexpr inline bool is_object<Tree> = true;

template<typename T>
concept object = is_object<T>;

using MutBlob = std::span<char>;
using MutTree = std::span<Handle>;
using MutObject = std::variant<MutBlob, MutTree>;

template<typename T>
constexpr inline bool is_mutable_object = false;

template<>
constexpr inline bool is_mutable_object<MutBlob> = true;

template<>
constexpr inline bool is_mutable_object<MutTree> = true;

template<typename T>
concept mutable_object = is_mutable_object<T>;

/**
 * A combination of std::span and std::unique_ptr; acts as a smart pointer for a span of memory of known
 * length.  Calls `malloc` on creation and `free` on deletion when used normally, but can also take ownership of an
 * already-allocated region or give up ownership to another owner.
 */
template<class S>
class Owned
{
public:
  enum class Deallocation
  {
    None,
    Free,
    Unmap,
  };

private:
  S span_;
  Deallocation deallocation_;

  Owned( S span, Deallocation deallocation )
    : span_( span )
    , deallocation_( deallocation )
  {}

public:
  Owned( size_t size )
  {
    span_ = {
      reinterpret_cast<S::value_type*>( malloc( size * sizeof( typename S::value_type ) ) ),
      size,
    };
    VLOG( 1 ) << "allocated " << size << " bytes at " << reinterpret_cast<void*>( span_.data() );
  }

  Owned( const std::filesystem::path path )
  {
    VLOG( 1 ) << "mapping " << path;
    size_t size = std::filesystem::file_size( path );
    int fd = open( path.c_str(), O_RDONLY );
    assert( fd >= 0 );
    void* p = mmap( NULL, size, PROT_READ, MAP_PRIVATE, fd, 0 );
    assert( p );
    span_ = { reinterpret_cast<S::value_type*>( p ), size };
    deallocation_ = Deallocation::Unmap;
    VLOG( 1 ) << "mapped " << size << " bytes at " << reinterpret_cast<void*>( span_.data() );
  }

  static Owned claim( S span, Deallocation deallocation )
  {
    VLOG( 1 ) << "claimed " << span.size() << " bytes at " << reinterpret_cast<void*>( span.data() );
    return Owned { span, deallocation };
  }

  Owned( Owned&& other )
    : span_( other.span_ )
    , deallocation_( other.deallocation_ )
  {
    other.release();
  }

  Deallocation deallocation() const { return deallocation_; }

  S::value_type* data() const { return span_.data(); };
  size_t size() const { return span_.size(); };

  S span() const { return span_; }
  operator S() const { return span(); }

  void release()
  {
    span_ = { reinterpret_cast<S::value_type*>( 0 ), 0 };
    deallocation_ = Deallocation::None;
  }

  Owned& operator=( Owned&& other )
  {
    this->span_ = other.span_;
    other.release();
    return *this;
  }

  S::value_type& operator[]( size_t index )
  {
    assert( index <= span_.size() );
    return span_[index];
  }

  S::value_type& at( size_t index ) { return ( *this )[index]; }

  ~Owned()
  {
    switch ( deallocation_ ) {
      case Deallocation::None:
        break;
      case Deallocation::Free:
        VLOG( 1 ) << "freeing " << span_.size() << " bytes at " << reinterpret_cast<void*>( span_.data() );
        free( span_.data() );
        break;
      case Deallocation::Unmap:
        VLOG( 1 ) << "unmapping " << span_.size() << " bytes at " << reinterpret_cast<void*>( span_.data() );
        munmap( span_.data(), span_.size() );
        break;
    }
    release();
  }
};

using OwnedBlob = Owned<MutBlob>;
using OwnedTree = Owned<MutTree>;
using OwnedObject = std::variant<OwnedBlob, OwnedTree>;

template<typename T>
constexpr inline bool is_owned_object = false;

template<>
constexpr inline bool is_owned_object<OwnedBlob> = true;

template<>
constexpr inline bool is_owned_object<OwnedTree> = true;

template<typename T>
concept owned_object = is_owned_object<T>;
