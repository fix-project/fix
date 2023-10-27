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
#include "sha256.hh"
#include "spans.hh"
#include "thunk.hh"

template<typename T>
constexpr inline bool is_span = false;

template<typename T>
constexpr inline bool is_span<std::span<T>> = true;

template<typename T>
concept any_span = is_span<T>;

template<typename T>
constexpr inline bool is_const_span = false;

template<typename T>
constexpr inline bool is_const_span<std::span<const T>> = true;

template<typename T>
concept const_span = is_const_span<T>;

template<any_span T>
size_t byte_size( T& span )
{
  return span.size() * sizeof( typename T::element_type );
}

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

enum class AllocationType
{
  Static,
  Allocated,
  Mapped,
};

template<any_span S>
class Owned
{
  S span_;
  AllocationType allocation_type_;

  Owned( S span, AllocationType allocation_type )
    : span_( span )
    , allocation_type_( allocation_type )
  {}

public:
  using span_type = S;
  using element_type = S::element_type;
  using value_type = S::value_type;
  using pointer = element_type*;
  using reference = element_type&;
  using const_pointer = const element_type*;
  using const_reference = element_type&;

  using mutable_span = std::span<value_type>;
  using const_span = std::span<const element_type>;

  static Owned allocate( size_t size )
    requires( not std::is_const_v<element_type> )
  {
    void* p = aligned_alloc( std::alignment_of<element_type>(), size * sizeof( element_type ) );
    CHECK( p );
    span_type span = {
      reinterpret_cast<pointer>( p ),
      size,
    };
    VLOG( 1 ) << "allocated " << size << " elements (" << size * sizeof( element_type ) << " bytes) at "
              << reinterpret_cast<void*>( span.data() );
    return {
      span,
      AllocationType::Allocated,
    };
  }

  static Owned map( size_t size )
    requires( not std::is_const_v<element_type> )
  {
    void* p = mmap( 0, size * sizeof( element_type ), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0 );
    if ( p == (void*)-1 ) {
      perror( "mmap" );
      CHECK( p != (void*)-1 );
    }
    CHECK( p );
    span_type span = {
      reinterpret_cast<pointer>( p ),
      size,
    };
    VLOG( 1 ) << "mapped " << size << " elements (" << size * sizeof( element_type ) << " bytes) at "
              << reinterpret_cast<void*>( span.data() );
    return {
      span,
      AllocationType::Mapped,
    };
  }

  static Owned claim_static( S span )
  {
    VLOG( 1 ) << "claimed " << byte_size( span ) << " static bytes at "
              << reinterpret_cast<const void*>( span.data() );
    return Owned { span, AllocationType::Static };
  }

  static Owned claim_allocated( S span )
  {
    VLOG( 1 ) << "claimed " << byte_size( span ) << " allocated bytes at "
              << reinterpret_cast<const void*>( span.data() );
    return Owned { span, AllocationType::Allocated };
  }

  static Owned claim_mapped( S span )
  {
    VLOG( 1 ) << "claimed " << byte_size( span ) << " mapped bytes at "
              << reinterpret_cast<const void*>( span.data() );
    return Owned { span, AllocationType::Mapped };
  }

  static Owned from_file( const std::filesystem::path path )
    requires std::is_const_v<element_type>
  {
    VLOG( 1 ) << "mapping " << path << " as read-only";
    size_t size = std::filesystem::file_size( path );
    int fd = open( path.c_str(), O_RDONLY );
    CHECK( fd >= 0 );
    void* p = mmap( NULL, size, PROT_READ | PROT_EXEC, MAP_SHARED, fd, 0 );
    CHECK( p );
    close( fd );
    return claim_mapped( { reinterpret_cast<pointer>( p ), size / sizeof( element_type ) } );
  }

  void to_file( const std::filesystem::path path )
    requires std::is_const_v<element_type>
  {
    VLOG( 1 ) << "writing " << path << " to disk";
    size_t bytes = byte_size( span_ );
    std::filesystem::resize_file( path, bytes );
    VLOG( 2 ) << "resized " << path << " to " << bytes;
    int fd = open( path.c_str(), O_RDWR );
    CHECK( fd >= 0 );
    VLOG( 2 ) << "mmapping " << path;
    void* p = mmap( NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
    if ( p == (void*)-1 ) {
      perror( "mmap" );
      CHECK( p != (void*)-1 );
    }
    CHECK( p );
    close( fd );
    VLOG( 2 ) << "memcpying " << reinterpret_cast<const void*>( span_.data() ) << " to " << p;
    memmove( p, const_cast<void*>( reinterpret_cast<const void*>( span_.data() ) ), bytes );
    VLOG( 2 ) << "mprotecting " << path;
    mprotect( p, bytes, PROT_READ | PROT_EXEC );

    // Transfer the old span to another object, which will deallocate it.
    auto discard = Owned { span_, allocation_type_ };

    span_ = { reinterpret_cast<pointer>( p ), span_.size() };
    allocation_type_ = AllocationType::Mapped;
  }

  Owned( Owned&& other )
    : span_( other.span_ )
    , allocation_type_( other.allocation_type_ )
  {
    other.leak();
  }

  AllocationType allocation_type() const { return allocation_type_; }

  pointer data() const { return span_.data(); };
  size_t size() const { return span_.size(); };

  span_type span() const { return span_; }

  operator span_type() const { return span(); }
  operator const_span() const
    requires( not std::is_const_v<element_type> )
  {
    return span();
  }

  Owned( Owned<std::span<value_type>>&& original )
    requires std::is_const_v<element_type>
    : span_( original.span() )
    , allocation_type_( original.allocation_type() )
  {
    original.leak();
    if ( allocation_type_ == AllocationType::Mapped ) {
      VLOG( 2 ) << "Setting allocation at " << reinterpret_cast<const void*>( span_.data() ) << " "
                << "(" << byte_size( span_ ) << " bytes) to RO.";
      int result = mprotect( const_cast<void*>( reinterpret_cast<const void*>( span_.data() ) ),
                             byte_size( span_ ),
                             PROT_READ | PROT_EXEC );
      if ( result != 0 ) {
        perror( "mprotect" );
        CHECK( result == 0 );
      }
    }
  };

  Owned& operator=( Owned<std::span<value_type>>&& original )
    requires std::is_const_v<element_type>
  {
    span_ = original.span();
    allocation_type_ = original.allocation_type();
    original.leak();
    if ( allocation_type_ == AllocationType::Mapped ) {
      VLOG( 2 ) << "Setting allocation at " << reinterpret_cast<const void*>( span_.data() ) << " "
                << "(" << byte_size( span_ ) << " bytes) to RO.";
      int result = mprotect( const_cast<void*>( reinterpret_cast<const void*>( span_.data() ) ),
                             byte_size( span_ ),
                             PROT_READ | PROT_EXEC );
      if ( result != 0 ) {
        perror( "mprotect" );
        CHECK( result == 0 );
      }
    }
    return *this;
  }

  void leak()
  {
    span_ = { reinterpret_cast<S::value_type*>( 0 ), 0 };
    allocation_type_ = AllocationType::Static;
  }

  Owned& operator=( Owned&& other )
  {
    this->span_ = other.span_;
    other.leak();
    return *this;
  }

  element_type& operator[]( size_t index )
  {
    CHECK( index < span_.size() );
    return span_[index];
  }

  element_type& at( size_t index ) { return ( *this )[index]; }

  ~Owned()
  {
    switch ( allocation_type_ ) {
      case AllocationType::Static:
        break;
      case AllocationType::Allocated:
        VLOG( 1 ) << "freeing " << span_.size() << " bytes at " << reinterpret_cast<const void*>( span_.data() );
        free( const_cast<void*>( reinterpret_cast<const void*>( span_.data() ) ) );
        break;
      case AllocationType::Mapped:
        VLOG( 1 ) << "unmapping " << span_.size() << " bytes at " << reinterpret_cast<const void*>( span_.data() );
        munmap( const_cast<void*>( reinterpret_cast<const void*>( span_.data() ) ), span_.size() );
        break;
    }
    leak();
  }
};

using OwnedMutBlob = Owned<MutBlob>;
using OwnedMutTree = Owned<MutTree>;
using OwnedMutObject = std::variant<OwnedMutBlob, OwnedMutTree>;

template<typename T>
constexpr inline bool is_owned_mut_object = false;

template<>
constexpr inline bool is_owned_mut_object<OwnedMutBlob> = true;

template<>
constexpr inline bool is_owned_mut_object<OwnedMutTree> = true;

template<typename T>
concept owned_mut_object = is_owned_mut_object<T>;

using OwnedBlob = Owned<Blob>;
using OwnedTree = Owned<Tree>;
using OwnedObject = std::variant<OwnedBlob, OwnedTree>;

template<typename T>
constexpr inline bool is_owned_object = false;

template<>
constexpr inline bool is_owned_object<OwnedBlob> = true;

template<>
constexpr inline bool is_owned_object<OwnedTree> = true;

template<typename T>
concept owned_object = is_owned_object<T>;
