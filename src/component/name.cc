#include "name.hh"

using namespace std;

ostream& operator<<( ostream& s, const Name name )
{
  s << hex << name.content_[0] << "|" << hex << name.content_[1] << "|" << hex << name.content_[2] << "|" << hex
    << name.content_[3];

  return s;
}
