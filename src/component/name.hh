#pragma once

#include <string>
#include <variant>
#include <vector>

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
  std::vector<int> path_;
};

class Name
{
  private:
    // Content of the name
    std::variant<std::string, ThunkRep> content_;
    // Type of the name
    NameType type_;
    // Type that the name points to
    ContentType content_type_;

  public:
    Name( const std::string & content, NameType type, ContentType content_type ) 
      : content_( content ),
        type_( type ),
        content_type_( content_type )
        
    {}

    Name( std::string && content, NameType type, ContentType content_type ) 
      : content_( std::move( content ) ),
        type_( type ),
        content_type_( content_type )
    {}

    Name()
      : content_( "" ),
        type_( NameType::Null ),
        content_type_( ContentType::Blob )
    {}

    Name( ContentType content_type )
      : content_( "" ),
        type_( NameType::Null ),
        content_type_( content_type )
    {}

    const std::string & getContent() const 
    { 
      if ( std::holds_alternative<std::string>( content_ ) )
      {
        return std::get<std::string>( content_ );
      } else
      {
        return std::get<ThunkRep>( content_ ).content_; 
      }
    }
    const NameType & getType() const { return type_; }
    const ContentType & getContentType() const { return content_type_; }
};



