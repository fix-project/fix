#include <span>
#include <string>

namespace sha256 {
std::string encode( std::span<const char> input );

bool verify( const std::string& ret, const std::string& input );
}
