#pragma once

#include <map>
#include <string>

#include <util.hh>

template<typename T>
class storage
{
  private: 
    std::map<std::string, T> name_to_object_;

  public:
    const T& get( std::string& name )
    {
      auto it = name_to_object.find( name );
      if ( it != name_to_object.end() )
      {
        return name_to_object[ name ];
      }
    
      std::string content = util::read_file( name );
      name_to_object.insert( { name, { name, content } } );
      
      return name_to_object[ name ];
    }

    void put( std::string& name, std::string&& content )
    {
      util::write_file( name, { content } );
      name_to_object.insert( { name, { name, content } } );
      return;
    }
}
