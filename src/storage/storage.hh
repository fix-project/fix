#pragma once

#include <iostream>
#include <map>
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

public:
  InMemoryStorage()
    : name_to_object_()
  {}

  const T& get( const Name name ) { return name_to_object_.at( name ); }

  T& getMutable( const Name name ) { return const_cast<T&>( get( name ) ); }

  void put( const Name name, T&& content )
  {
    name_to_object_.try_emplace( name, std::move( content ) );
    return;
  }

  size_t size() { return name_to_object_.size(); }

  bool contains( const Name name ) { return name_to_object_.contains( name ); }
};
