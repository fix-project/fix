#include "name.hh"

using namespace std;

bool operator==( const Name& lhs, const Name& rhs )
{
  return ( lhs.getType() == rhs.getType() && lhs.getContent() == rhs.getContent() );
}

ostream& operator<<( ostream& s, const Name& name )
{
  switch ( name.getContentType() ) {
    case ContentType::Blob:
      s << "BLOB ";
      break;

    case ContentType::Tree:
      s << "TREE ";
      break;

    case ContentType::Thunk:
      s << "THUNK ";
      break;

    case ContentType::Unknown:
      s << "UNKNOWN ";
      break;

    default:
      break;
  }

  s << name.getContent() << " ";
  for ( const auto& ch : name.getContent() ) {
    s << hex << (int)ch;
  }

  s << endl;

  return s;
}
