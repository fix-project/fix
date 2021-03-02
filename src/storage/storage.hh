#pragma once

#include <iostream>
#include <map>
#include <unordered_map>
#include <string>

#include "absl/container/flat_hash_map.h"
#include <util.hh>

template<typename T>
class Storage
{
  public:
    virtual const T& get( const std::string & name ) = 0;
    virtual T& getMutable( const std::string & name ) = 0;
    virtual void put( const std::string & name, T && content ) = 0;
    virtual ~Storage() {};
};

template<typename T>
class InMemoryStorage : public Storage<T>
{
  private:
    absl::flat_hash_map<std::string, T> name_to_object_;
  
  public:
    InMemoryStorage()
    : name_to_object_()
    {}

    const T& get( const std::string & name )
    {
      return name_to_object_.at( name );
    }

    T& getMutable( const std::string & name )
    {
      return const_cast<T &>( get( name ) );
    }

    void put( const std::string& name, T && content )
    {
      name_to_object_.try_emplace( name, std::move( content ) );
      return;
    }

    size_t size()
    {
      return name_to_object_.size();
    }
};
