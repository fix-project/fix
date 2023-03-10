#pragma once

#include <iostream>
#include <map>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "name.hh"

template<typename T>
class Storage
{
public:
  virtual const T& get( const Name name ) = 0;
  virtual T& getMutable( const Name name ) = 0;
  virtual void put( const Name name, T&& content ) = 0;
  virtual ~Storage() {};
};

template<typename T>
class InMemoryStorage : public Storage<T>
{
private:
  absl::flat_hash_map<Name, T, NameHash> name_to_object_;
  std::shared_mutex name_to_object_mutex_;

public:
  InMemoryStorage()
    : name_to_object_()
    , name_to_object_mutex_()
  {}

  const T& get( const Name name )
  {
    std::shared_lock lock( name_to_object_mutex_ );
    return name_to_object_.at( name );
  }

  T& getMutable( const Name name )
  {
    std::shared_lock lock( name_to_object_mutex_ );
    return const_cast<T&>( get( name ) );
  }

  void put( const Name name, T&& content )
  {
    std::unique_lock lock( name_to_object_mutex_ );
    name_to_object_.try_emplace( name, std::move( content ) );
    return;
  }

  size_t size()
  {
    std::shared_lock lock( name_to_object_mutex_ );
    return name_to_object_.size();
  }

  bool contains( const Name name )
  {
    std::shared_lock lock( name_to_object_mutex_ );
    return name_to_object_.contains( name );
  }
};
