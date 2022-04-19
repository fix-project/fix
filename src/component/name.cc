#include "name.hh"

using namespace std;

bool operator==( const Name& lhs, const Name& rhs )
{
  auto neq = _mm256_xor_si256( lhs.content_, rhs.content_ );
  return _mm256_testz_si256( neq, neq );
}

ostream& operator<<( ostream& s, const Name& name )
{
  s << std::bitset<8>( name.metadata() ) << endl;
  s << hex << name.content_[0] << " " << name.content_[1] << " " << name.content_[2] << " " << name.content_[3]
    << endl;

  return s;
}
