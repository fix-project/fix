#pragma once
#include <concepts>
#include <variant>

#include "handle.hh"
#include "types.hh"

enum class FixKind
{
  Value,
  Object,
  Expression,
  Fix,
};

namespace handle {
template<typename S, typename T>
inline std::optional<Handle<S>> extract( Handle<T> original )
{
  if constexpr ( std::same_as<S, T> ) {
    return original;
  } else if constexpr ( std::same_as<T, ValueTreeRef> or std::same_as<T, ObjectTreeRef>
                        or std::same_as<T, BlobRef> ) {
    return {};
  } else if constexpr ( Handle<T>::is_fix_sum_type ) {
    return std::visit( []( const auto x ) { return extract<S>( x ); }, original.get() );
  } else {
    return {};
  }
}

template<typename T>
static inline Handle<AnyDataType> data( Handle<T> handle )
{
  if constexpr ( std::same_as<T, Relation> ) {
    return handle;
  } else if constexpr ( std::same_as<T, ValueTreeRef> or std::same_as<T, ObjectTreeRef>
                        or std::same_as<T, BlobRef> ) {
    __builtin_unreachable();
  } else if constexpr ( not Handle<T>::is_fix_sum_type ) {
    return handle;
  } else {
    return std::visit( []( const auto x ) { return data( x ); }, handle.get() );
  }
}

template<typename T>
static inline bool is_local( Handle<T> handle )
{
  return std::visit(
    []( auto x ) {
      if constexpr ( std::same_as<decltype( x ), Handle<Relation>> ) {
        return x.template visit<bool>( []( const auto x ) { return is_local( x ); } );
      } else {
        return x.is_local();
      }
    },
    data( handle ).get() );
}

template<typename T>
static inline size_t local_name( Handle<T> handle )
{
  return std::visit( []( const auto x ) { return x.local_name(); }, data( handle ).get() );
}

template<typename T>
static inline size_t size( Handle<T> handle )
{
  if constexpr ( std::same_as<T, Relation> ) {
    return sizeof( Handle<Fix> );
  } else if constexpr ( not Handle<T>::is_fix_sum_type ) {
    return handle.size();
  } else {
    return std::visit( []( const auto x ) { return size( x ); }, handle.get() );
  }
}

template<typename T>
static inline size_t byte_size( Handle<T> handle )
{
  if constexpr ( std::same_as<T, Relation> ) {
    return sizeof( Handle<Fix> );
  } else if constexpr ( std::same_as<T, ValueTreeRef> or std::same_as<T, ObjectTreeRef> ) {
    return 0;
  } else if constexpr ( std::same_as<T, Thunk> ) {
    return sizeof( Handle<Fix> );
  } else if constexpr ( not Handle<T>::is_fix_sum_type ) {
    return handle.size();
  } else {
    return std::visit( []( const auto x ) { return byte_size( x ); }, handle.get() );
  }
}

template<typename T>
FixKind kind( Handle<T> handle )
{
  if constexpr ( Handle<T>::is_fix_sum_type and not Handle<T>::is_fix_wrapper ) {
    return std::visit( []( const auto x ) { return handle::kind( x ); }, handle.get() );
  } else {
    if constexpr ( std::convertible_to<Handle<T>, Handle<Value>> ) {
      return FixKind::Value;
    } else if constexpr ( std::convertible_to<Handle<T>, Handle<Object>> ) {
      return FixKind::Object;
    } else if constexpr ( std::convertible_to<Handle<T>, Handle<Expression>> ) {
      return FixKind::Expression;
    } else if constexpr ( std::convertible_to<Handle<T>, Handle<Fix>> ) {
      return FixKind::Fix;
    }
  }
}

template<typename T>
static inline Handle<Fix> fix( Handle<T> handle )
{
  return handle.template visit<Handle<Fix>>( []( auto x ) { return x; } );
}

template<FixTreeType T>
Handle<T> tree_unwrap( Handle<AnyTree> handle )
{
  return handle.visit<Handle<T>>( [&]( auto t ) -> Handle<T> {
    if constexpr ( std::constructible_from<Handle<T>, decltype( t )> ) {
      return Handle<T>( t );
    } else {
      throw std::runtime_error( "tried to create tree with incorrect type" );
    }
  } );
}

template<FixTreeType T>
Handle<T> tree_unwrap( Handle<Expression> handle )
{
  return data( handle ).visit<Handle<T>>( [&]( auto t ) -> Handle<T> {
    if constexpr ( std::constructible_from<Handle<T>, decltype( t )> ) {
      return Handle<T>( t );
    } else {
      throw std::runtime_error( "tried to create tree with incorrect type" );
    }
  } );
}

static inline Handle<ExpressionTree> upcast( Handle<AnyTree> tree )
{
  return tree.visit<Handle<ExpressionTree>>(
    []( auto t ) -> Handle<ExpressionTree> { return Handle<ExpressionTree>( t ); } );
}
}
