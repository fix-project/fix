#include <string>

namespace sha256
{
  std::string encode( const std::string & input );
  bool verify( const std::string & ret, const std::string & input ); 
}
