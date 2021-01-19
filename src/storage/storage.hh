#pragma once

#include <map>
#include <string>

#include <util.hh>

template<typename T>
class Storage
{
  public:
    virtual const T& get( const std::string & name ) = 0;
    virtual T& getMutable( const std::string & name ) = 0;
    virtual void put( const std::string & name, std::string && content ) = 0;
    virtual ~Storage() {};
};

template<typename T>
class InMemoryStorage : public Storage<T>
{
  private:
    std::map<std::string, T> name_to_object_;
  
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

    void put( const std::string& name, std::string&& content )
    {
      name_to_object_.insert( std::pair<std::string,T>( name, T( name, std::move(content) ) ) );
      return;
    }
};
