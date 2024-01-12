#pragma once

#include <filesystem>
#include <span>

#include "handle.hh"

struct Blob : public std::span<const char>
{
  using std::span<const char>::span;
};

template<typename T>
struct Tree : public std::span<const Handle<T>>
{
  using std::span<const Handle<T>>::span;
};

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

  Owned( S span, AllocationType allocation_type );

public:
  using span_type = S;
  using element_type = S::element_type;
  using value_type = S::value_type;
  using pointer = element_type*;
  using reference = element_type&;
  using const_pointer = const element_type*;
  using const_reference = const element_type&;

  using mutable_span = std::span<value_type>;
  using const_span = std::span<const element_type>;

  Owned( Owned&& other );

  Owned( Owned<std::span<value_type>>&& original ) requires std::is_const_v<element_type>;

  static Owned allocate( size_t size ) requires( not std::is_const_v<element_type> );
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

  operator span_type() const { return span(); }
  operator const_span() const requires( not std::is_const_v<element_type> ) { return span(); }

  Owned& operator=( Owned<std::span<value_type>>&& original ) requires std::is_const_v<element_type>;
  Owned& operator=( Owned&& other );

  element_type& operator[]( size_t index );
  element_type& at( size_t index ) { return ( *this )[index]; }

  ~Owned();
};

using OwnedBlob = Owned<Blob>;
template<typename T>
using OwnedTree = Owned<Tree<T>>;
