#pragma once

#include <iostream>
#include <string>
#include <variant>
#include <vector>

#include "absl/hash/hash.h"

enum class NameType
{
  Literal,
  Canonical,
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

  std::vector<size_t> path_;
  std::vector<Name> tree_content_;

public:
  Name( const std::string& content, NameType type, ContentType content_type )
    : content_( content )
    , type_( type )
    , content_type_( content_type )
    , path_()
    , tree_content_()
  {}

  Name( std::string&& content, NameType type, ContentType content_type )
    : content_( std::move( content ) )
    , type_( type )
    , content_type_( content_type )
    , path_()
    , tree_content_()
  {}

  Name()
    : content_( "" )
    , type_( NameType::Null )
    , content_type_( ContentType::Unknown )
    , path_()
    , tree_content_()
  {}

  Name( ContentType content_type )
    : content_( "" )
    , type_( NameType::Null )
    , content_type_( content_type )
    , path_()
    , tree_content_()
  {}

  Name( const Name& encode_name, std::vector<size_t> path, ContentType content_type )
    : content_( "" )
    , type_( NameType::Thunk )
    , content_type_( content_type )
    , path_( path )
    , tree_content_( { encode_name } )
  {}

  Name( const std::vector<Name>& tree_content )
    : content_( "" )
    , type_( NameType::Literal )
    , content_type_( ContentType::Tree )
    , path_()
    , tree_content_( tree_content )
  {}

  const std::string& getContent() const { return content_; }

  const std::vector<Name>& getTreeContent() const { return tree_content_; }

  const NameType& getType() const { return type_; }
  const ContentType& getContentType() const { return content_type_; }
  const std::vector<size_t>& getPath() const { return path_; }

  template<typename H>
  friend H AbslHashValue( H h, const Name& name )
  {
    return H::combine( std::move( h ), name.type_, name.content_, name.tree_content_, name.path_ );
  }
};

bool operator==( const Name& lhs, const Name& rhs );
std::ostream& operator<<( std::ostream& s, const Name& name );
