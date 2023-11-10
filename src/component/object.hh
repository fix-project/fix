#pragma once

#include <filesystem>
#include <span>

#include "handle.hh"

template<typename T>
constexpr inline bool is_span = false;

template<typename T>
constexpr inline bool is_span<std::span<T>> = true;

template<typename T>
concept any_span = is_span<T>;

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

using OwnedMutBlob = Owned<MutBlob>;
using OwnedMutTree = Owned<MutTree>;
using OwnedMutObject = std::variant<OwnedMutBlob, OwnedMutTree>;

using OwnedBlob = Owned<Blob>;
using OwnedTree = Owned<Tree>;
using OwnedObject = std::variant<OwnedBlob, OwnedTree>;
