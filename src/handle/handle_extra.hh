#pragma once
#include <variant>

#include "blob.hh"      // IWYU pragma: export
#include "literals.hh"  // IWYU pragma: export
#include "operators.hh" // IWYU pragma: export
#include "tree.hh"      // IWYU pragma: export
#include "types.hh"     // IWYU pragma: export

struct Blob;

template<typename S, typename T>
inline std::optional<Handle<S>> fix_extract( Handle<T> original )
{
  if constexpr ( std::same_as<S, T> ) {
    return original;
  } else if constexpr ( Handle<T>::is_fix_sum_type ) {
    return std::visit( []( const auto x ) { return fix_extract<S>( x ); }, original.get() );
  } else {
    return {};
  }
}

using FixDataType = std::variant<Handle<Literal>,
                                 Handle<Named>,
                                 Handle<ObjectTree>,
                                 Handle<ValueTree>,
                                 Handle<ExpressionTree>,
                                 Handle<FixTree>>;
template<typename T>
static inline FixDataType fix_data( Handle<T> handle )
{
  if constexpr ( not Handle<T>::is_fix_sum_type ) {
    return handle;
  } else {
    return std::visit( []( const auto x ) { return fix_data( x ); }, handle.get() );
  }
}

template<typename T>
static inline bool fix_is_local( Handle<T> handle )
{
  return std::visit( []( const auto x ) { return x.is_local(); }, fix_data( handle ) );
}

template<typename T>
static inline size_t fix_size( Handle<T> handle )
{
  return std::visit( []( const auto x ) -> size_t { return x.size(); }, fix_data( handle ) );
}

enum class FixKind
{
  Object,
  Value,
  Expression,
  Fix,
};

template<typename T>
FixKind fix_kind( Handle<T> handle )
{
  if constexpr ( Handle<T>::is_fix_sum_type and not Handle<T>::is_fix_wrapper ) {
    return std::visit( []( const auto x ) { return fix_kind( x ); }, handle.get() );
  } else {
    if constexpr ( std::convertible_to<Handle<T>, Handle<Object>> ) {
      return FixKind::Object;
    } else if constexpr ( std::convertible_to<Handle<T>, Handle<Value>> ) {
      return FixKind::Value;
    } else if constexpr ( std::convertible_to<Handle<T>, Handle<Expression>> ) {
      return FixKind::Expression;
    } else if constexpr ( std::convertible_to<Handle<T>, Handle<Fix>> ) {
      return FixKind::Fix;
    }
  }
}
