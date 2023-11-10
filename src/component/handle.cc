#include "handle.hh"
#include "base16.hh"

using namespace std;

ostream& operator<<( ostream& s, const Laziness laziness )
{
  switch ( laziness ) {
    case Laziness::Lazy:
      s << "Lazy";
      break;
    case Laziness::Shallow:
      s << "Shallow";
      break;
    case Laziness::Strict:
      s << "Strict";
      break;
  }
  return s;
}

ostream& operator<<( ostream& s, const ContentType content_type )
{
  switch ( content_type ) {
    case ContentType::Blob:
      s << "Blob";
      break;
    case ContentType::Tree:
      s << "Tree";
      break;
    case ContentType::Tag:
      s << "Tag";
      break;
    case ContentType::Thunk:
      s << "Thunk";
      break;
  }
  return s;
}

string escape( std::span<const char> data )
{
  char chars[17] = "0123456789abcdef";
  string x;
  for ( char i : data ) {
    if ( isprint( i ) ) {
      x += i;
    } else {
      x += "\\x";
      x += chars[i >> 4];
      x += chars[i & 0xf];
    }
  }
  return x;
}

ostream& operator<<( ostream& s, const Handle handle )
{
  s << handle.get_content_type();
  s << " { ";
  s << handle.get_laziness();
  s << ", ";
  if ( handle.is_literal_blob() ) {
    s << "Literal=\"" << escape( handle.literal_blob() );
    s << "\", ";

  } else if ( handle.is_local() ) {
    s << "Local=" << handle.get_local_id();
    s << ", ";
  } else {
    s << "Canonical=" << base16::encode( handle );
    s << ", ";
  }
  s << "Length=" << handle.get_length();
  s << " }";

  return s;
}
