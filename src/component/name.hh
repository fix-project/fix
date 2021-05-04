#pragma once

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
  Thunk
};

struct ThunkRep
{
  std::string content_;
  std::vector<size_t> path_;

  ThunkRep( const std::string & content, const std::vector<size_t> & path )
  : content_( content ),
    path_( path )
  {}
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

  public:
    Name( const std::string & content, NameType type, ContentType content_type ) 
      : content_( content ),
        type_( type ),
        content_type_( content_type ),
	path_()
        
    {}

    Name( std::string && content, NameType type, ContentType content_type ) 
      : content_( std::move( content ) ),
        type_( type ),
        content_type_( content_type ),
	path_()
    {}

    Name()
      : content_( "" ),
        type_( NameType::Null ),
        content_type_( ContentType::Blob ),
	path_()
    {}

    Name( ContentType content_type )
      : content_( "" ),
        type_( NameType::Null ),
        content_type_( content_type ),
	path_()
    {}

    Name ( const Name & encode_name, std::vector<size_t> path, ContentType content_type )
    : content_( encode_name.getContent() ),
      type_( NameType::Thunk ),
      content_type_( content_type ),
      path_( path )
    {}

    const std::string & getContent() const 
    { 
      return content_;
    }
    const NameType & getType() const { return type_; }
    const ContentType & getContentType() const { return content_type_; }

    template <typename H>
    friend H AbslHashValue( H h, const Name & name )
    { 
      //switch ( name.type_ )
      //{
      //  case NameType::Thunk :
      //    return H::combine( std::move( h ), std::get<ThunkRep>( name.content_ ), name.content_type_ );
        
      //  default :
      //    return H::combine(std::move( h ), std::get<std::string>( name.content_ ), name.content_type_ );
      //}
      return H::combine( std::move( h ), name.content_type_ );
    }

};

bool operator==( const Name & lhs, const Name & rhs );


