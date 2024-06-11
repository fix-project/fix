#include "handle.hh"

#include <atomic>
#include <functional>
#include <optional>
#include <vector>

template<FixType T, class V, class Hash = std::hash<Handle<T>>, class KeyEqual = std::equal_to<Handle<T>>>
class FixTable
{
  struct tEntry
  {
    Handle<T> h {};
    V v {};
    std::atomic<bool> occupied {};
  };

  std::vector<tEntry> data_;

  std::optional<size_t> get_idx( const Handle<T> h ) const
  {
    Hash hash;
    auto init_idx = hash( h ) % data_.size();
    auto idx = init_idx;

    while ( true ) {
      auto occupied = data_.at( idx ).occupied.load( std::memory_order_acquire );
      if ( occupied ) {
        KeyEqual eq;
        if ( eq( data_.at( idx ).h, h ) ) {
          return idx;
        }

        idx++;
        if ( idx == data_.size() ) {
          idx = 0;
        }

        if ( idx == init_idx ) {
          return {};
        }
      } else {
        return {};
      }
    }
  }

  size_t find_first_unoccupied( size_t idx )
  {
    bool expected = false;
    auto init_idx = idx;

    while ( true ) {
      if ( data_.at( idx ).occupied.compare_exchange_strong( expected, true, std::memory_order_release ) ) {
        return idx;
      } else {
        idx++;
        if ( idx == data_.size() ) {
          idx = 0;
        }

        if ( idx == init_idx ) {
          throw std::runtime_error( "Hash table is full." );
        }
      }
    }
  }

public:
  FixTable( size_t s )
    : data_( s )
  {}

  void insert( const Handle<T> h, V v )
  {
    Hash hash;
    auto idx = hash( h ) % data_.size();

    while ( true ) {
      auto occupied = data_.at( idx ).occupied.load( std::memory_order_acquire );
      if ( occupied ) {
        KeyEqual eq;
        if ( eq( data_.at( idx ).h, h ) ) {
          return;
        }

        idx++;
        if ( idx == data_.size() ) {
          idx = 0;
        }
      } else {
        auto empty_slot = find_first_unoccupied( idx );
        data_.at( empty_slot ).h = h;
        data_.at( empty_slot ).v = v;
        return;
      }
    }
  }

  bool contains( const Handle<T> h ) const { return get_idx( h ).has_value(); }

  std::optional<V> get( const Handle<T> h ) const
  {
    return get_idx( h ).transform( [&]( auto idx ) { return data_.at( idx ).v; } );
  }

  std::optional<Handle<T>> get_handle( const Handle<T> h ) const
  {
    return get_idx( h ).transform( [&]( auto idx ) { return data_.at( idx ).h; } );
  }
};
