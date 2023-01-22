#pragma once

#include "absl/container/flat_hash_map.h"
#include "name.hh"

template<typename T>
class concurrent_flat_hash_map
{
private:
  absl::flat_hash_map<Name, T, NameHash> flat_hash_map_;
  std::mutex flat_hash_map_mutex_;

public:
  concurrent_flat_hash_map()
    : flat_hash_map_()
    , flat_hash_map_mutex_()
  {}

  T& at( Name name )
  {
    std::lock_guard<std::mutex> lock( flat_hash_map_mutex_ );
    return flat_hash_map_.at( name );
  }

  bool contains( const Name name )
  {
    std::lock_guard<std::mutex> lock( flat_hash_map_mutex_ );
    return flat_hash_map_.contains( name );
  }

  void insert_or_assign( const Name name, const T& value )
  {
    std::lock_guard<std::mutex> lock( flat_hash_map_mutex_ );
    flat_hash_map_.insert_or_assign( name, std::move( value ) );
  }

  void insert_or_assign( const Name name, T&& value )
  {
    std::lock_guard<std::mutex> lock( flat_hash_map_mutex_ );
    flat_hash_map_.insert_or_assign( name, std::move( value ) );
  }
};
