#pragma once

#include <filesystem>
#include <span>

#include "handle.hh"

using BlobSpan = std::span<const char>;
using TreeSpan = std::span<const Handle<Fix>>;
using Span = std::variant<BlobSpan, TreeSpan>;

using MutBlobSpan = std::span<char>;
using MutTreeSpan = std::span<Handle<Fix>>;
using MutSpan = std::variant<MutBlobSpan, MutTreeSpan>;

enum class AllocationType
{
  Static,
  Allocated,
  Mapped,
};

template<typename S>
class Owned
{
  S span_;
  AllocationType allocation_type_;

public:
  using span_type = S;
  using element_type = S::element_type;
  using value_type = S::value_type;
  using pointer = element_type*;
  using reference = element_type&;

  using const_pointer = const element_type*;
  using const_reference = const element_type&;
  using const_span = std::span<const element_type>;

  using mutable_pointer = value_type*;
  using mutable_reference = value_type&;
  using mutable_span = std::span<value_type>;

  Owned( S span, AllocationType allocation_type );
  Owned( Owned& other ) = delete;
  Owned( Owned&& other );

  Owned( Owned<std::span<value_type>>&& original ) requires std::is_const_v<element_type>;
  Owned( std::filesystem::path path ) requires std::is_const_v<element_type>;
  Owned( size_t size, AllocationType type ) requires( not std::is_const_v<element_type> );

  static Owned allocate( size_t size ) requires( not std::is_const_v<element_type> )
  {
    return Owned( size, AllocationType::Allocated );
  }
  static Owned map( size_t size ) requires( not std::is_const_v<element_type> )
  {
    return Owned( size, AllocationType::Mapped );
  }

  void to_file( const std::filesystem::path path ) requires std::is_const_v<element_type>;

  void leak();

  span_type span() const { return span_; }
  AllocationType allocation_type() const { return allocation_type_; }

  pointer data() const { return span_.data(); };
  size_t size() const { return span_.size(); };

  Owned& operator=( Owned<std::span<value_type>>&& original ) requires std::is_const_v<element_type>;
  Owned& operator=( Owned&& other );
  Owned& operator=( Owned& other ) = delete;

  element_type& operator[]( size_t index );
  element_type& at( size_t index ) { return ( *this )[index]; }

  ~Owned();
};

using OwnedBlob = Owned<BlobSpan>;
using OwnedTree = Owned<TreeSpan>;
using OwnedMutBlob = Owned<MutBlobSpan>;
using OwnedMutTree = Owned<MutTreeSpan>;

using OwnedSpan = std::variant<OwnedBlob, OwnedTree>;
using OwnedMutSpan = std::variant<OwnedMutBlob, OwnedMutTree>;

using BlobData = std::shared_ptr<OwnedBlob>;
using TreeData = std::shared_ptr<OwnedTree>;
using Data = std::variant<BlobData, TreeData>;
