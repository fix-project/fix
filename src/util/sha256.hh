#include <string>
#include <string_view>

namespace sha256 {
std::string encode( const std::string& input );

std::string encode( std::string_view input );

bool verify( const std::string& ret, const std::string& input );
}
