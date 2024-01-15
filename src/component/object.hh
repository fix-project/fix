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
  Owned( Owned&& other );

  Owned( Owned<std::span<value_type>>&& original ) requires std::is_const_v<element_type>;

  static Owned<S> allocate( size_t size ) requires( not std::is_const_v<element_type> );
  static Owned map( size_t size ) requires( not std::is_const_v<element_type> );

  static Owned claim_static( S span );
  static Owned claim_allocated( S span );
  static Owned claim_mapped( S span );

  static Owned from_file( const std::filesystem::path path ) requires std::is_const_v<element_type>;
  void to_file( const std::filesystem::path path ) requires std::is_const_v<element_type>;
  static Owned copy( std::span<element_type> data );

  void leak();

  span_type span() const { return span_; }
  AllocationType allocation_type() const { return allocation_type_; }

  pointer data() const { return span_.data(); };
  size_t size() const { return span_.size(); };

  Owned& operator=( Owned<std::span<value_type>>&& original ) requires std::is_const_v<element_type>;
  Owned& operator=( Owned&& other );

  element_type& operator[]( size_t index );

  ~Owned();
};

using OwnedBlob = Owned<BlobSpan>;
using OwnedTree = Owned<TreeSpan>;
using OwnedMutBlob = Owned<MutBlobSpan>;
using OwnedMutTree = Owned<MutTreeSpan>;

using OwnedSpan = std::variant<OwnedBlob, OwnedTree>;
using OwnedMutSpan = std::variant<OwnedMutBlob, OwnedMutTree>;
