#pragma once

#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <variant>

#include "name.hh"
#include "spans.hh"
#include "thunk.hh"

template<typename T>
struct OptionalFree
{
  bool own { true };

  void operator()( T* ptr ) const
  {
    if ( own ) {
      free( ptr );
    };
  }
};

template<typename T, typename View>
class Value
{
public:
  using ptr_type = std::unique_ptr<T, OptionalFree<T>>;

  Value( ptr_type&& data, const uint32_t size )
    : data_( std::move( data ) )
    , size_( size )
  {
  }

  Value( View str )
    : data_( const_cast<T*>( str.data() ) )
    , size_( str.size() )
  {
    data_.get_deleter().own = false;
    if ( str.size() > std::numeric_limits<uint32_t>::max() ) {
      throw std::out_of_range( "Value size too big" );
    }
  }

  Value( const Value& ) = delete;
  Value& operator=( const Value& ) = delete;

  Value( Value&& other ) = default;
  Value& operator=( Value&& other ) = default;

  operator View() const { return { data_.get(), size_ }; }
  const T* data() const { return data_.get(); }
  uint32_t size() const { return size_; }

private:
  ptr_type data_;
  uint32_t size_;
};

static_assert( sizeof( __m256i ) == sizeof( Name ) );

using Blob = Value<char, std::string_view>;
using Tree = Value<Name, span_view<Name>>;
using Object = std::variant<Blob, Tree>;

using Blob_ptr = Blob::ptr_type;
using Tree_ptr = Tree::ptr_type;

template<typename T>
Blob make_blob( const T& t )
{
  Blob_ptr t_storage { static_cast<char*>( malloc( sizeof( T ) ) ) };
  if ( not t_storage ) {
    throw std::bad_alloc();
  }
  memcpy( t_storage.get(), &t, sizeof( T ) );
  return { std::move( t_storage ), sizeof( T ) };
}
