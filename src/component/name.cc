#include "name.hh"

using namespace std;

ostream& operator<<( ostream& s, const Name name )
{
  s << hex << name.content_[0];
  s << "|";
  s << hex << name.content_[1];
  s << "|";
  s << hex << name.content_[2];
  s << "|";
  s << hex << name.content_[3];

  return s;
}
