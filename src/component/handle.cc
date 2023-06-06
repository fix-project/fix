#include "handle.hh"

using namespace std;

ostream& operator<<( ostream& s, const Handle handle )
{
  s << hex << handle.content_[0] << "|" << hex << handle.content_[1] << "|" << hex << handle.content_[2] << "|"
    << hex << handle.content_[3];

  return s;
}
