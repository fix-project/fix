#pragma once

#include <span>
#include <string>

namespace sha256 {

std::string encode( std::string_view input );

template<class T>
std::string encode_span( std::span<T> input ) requires( std::is_const_v<T> )
{
  return encode( { reinterpret_cast<const char* const>( input.data() ), input.size() * sizeof( T ) } );
}

bool verify( const std::string& ret, const std::string& input );
}
