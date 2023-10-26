#pragma once

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <variant>

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
  S span_;

  Owned( S span )
    : span_( span )
  {}

public:
  Owned( size_t size )
  {
    span_ = {
      reinterpret_cast<S::value_type*>( malloc( size * sizeof( typename S::value_type ) ) ),
      size,
    };
  }

  static Owned claim( S span ) { return Owned( span ); }

  Owned( Owned&& other )
    : span_( other.span_ )
  {
    other.release();
  }

  S::value_type* data() const { return span_.data(); };
  size_t size() const { return span_.size(); };

  S span() const { return span_; }
  operator S() const { return span(); }

  void release() { span_ = { reinterpret_cast<S::value_type*>( 0 ), 0 }; }

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

  S::value_type& at( size_t index ) { return this[index]; }

  ~Owned()
  {
    free( span_.data() );
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
