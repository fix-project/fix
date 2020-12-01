#include <string>

using namespace std

namespace sha256
{
  string encode( const std::string & input );
  bool verify( const std::string & ret, const std::string & input ) 
}
