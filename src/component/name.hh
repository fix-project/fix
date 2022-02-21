#pragma once

#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "absl/hash/hash.h"

class Name;

enum class NameType
{
  Canonical,
  Literal,
  Local,
  Thunk,
  Null
};

enum class ContentType
{
  Blob,
  Tree,
  Thunk,
  Unknown
};

class Name
{
private:
  // Content of the name
  std::string content_;
  // Type of the name
  NameType type_;
  // Type that the name points to
  ContentType content_type_;

public:
  Name( const std::string& content, NameType type, ContentType content_type )
    : content_( content )
    , type_( type )
    , content_type_( content_type )
  {}

  Name( std::string&& content, NameType type, ContentType content_type )
    : content_( std::move( content ) )
    , type_( type )
    , content_type_( content_type )
  {}

  Name()
    : content_( "" )
    , type_( NameType::Null )
    , content_type_( ContentType::Unknown )
  {}

  Name( ContentType content_type )
    : content_( "" )
    , type_( NameType::Null )
    , content_type_( content_type )
  {}

  // Name( const Name& encode_name, ContentType content_type )
  //   : content_( "" )
  //   , type_( NameType::Thunk )
  //   , content_type_( content_type )
  //{}

  const std::string& getContent() const { return content_; }

  const NameType& getType() const { return type_; }
  const ContentType& getContentType() const { return content_type_; }

  template<typename H>
  friend H AbslHashValue( H h, const Name& name )
  {
    return H::combine( std::move( h ), name.type_, name.content_ );
  }
};

bool operator==( const Name& lhs, const Name& rhs );
std::ostream& operator<<( std::ostream& s, const Name& name );
