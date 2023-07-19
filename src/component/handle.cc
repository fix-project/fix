#include "handle.hh"

using namespace std;

ostream& operator<<( ostream& s, const Handle handle )
{
  s << "&";
  switch ( handle.get_laziness() ) {
    case Laziness::Lazy:
      s << "lazy ";
      break;
    case Laziness::Shallow:
      s << "shallow ";
      break;
    case Laziness::Strict:
      s << "strict ";
      break;
  }
  switch ( handle.get_content_type() ) {
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
  s << "(";
  s << hex << handle.content_[0] << "|" << hex << handle.content_[1] << "|" << hex << handle.content_[2] << "|"
    << hex << handle.content_[3];
  s << ")";

  if ( handle.is_literal_blob() )
    s << "!";

  return s;
}
