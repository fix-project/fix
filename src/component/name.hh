#pragma once

#include <string>

enum class NameType 
{
  Literal,
  Canonical
};

enum class ContentType
{
  Blob,
  Tree,
  Thunk
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
        type_( NameType::Literal ),
        content_type_( ContentType::Blob )
    {}

    const std::string & getContent() const { return content_; }
    const NameType & getType() const { return type_; }
    const ContentType & getContentType() const { return content_type_; }
};



