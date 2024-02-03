#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <string>

#include "exception.hh"

class Parser
{
  bool error_ {};
  std::string_view input_;

  void check_size( const size_t size )
  {
    if ( size > input_.size() ) {
      set_error();
    }
  }

public:
  Parser( const std::string_view input )
    : input_( input )
  {}

  std::string_view input() const { return input_; }
  bool error() const { return error_; }
  void set_error() { error_ = true; }

  template<typename T>
  void integer( T& out )
  {
    check_size( sizeof( T ) );
    if ( error() ) {
      return;
    }

    memcpy( &out, input_.data(), sizeof( T ) );
    input_.remove_prefix( sizeof( T ) );
  }

  void string( std::span<char> out )
  {
    check_size( out.size() );
    if ( error() ) {
      return;
    }
    memcpy( out.data(), input_.data(), out.size() );
    input_.remove_prefix( out.size() );
  }

  template<typename T>
  void object( T& out )
  {
    if ( not error() ) {
      out.parse( *this );
    }
  }
};

class Serializer
{
  std::span<char> output_;
  size_t original_size_;

  void check_size( const size_t size )
  {
    if ( size > output_.size() ) {
      throw std::runtime_error( "no room to serialize" );
    }
  }

public:
  Serializer( std::span<char> output )
    : output_( output )
    , original_size_( output.size() )
  {}

  size_t bytes_written() const { return original_size_ - output_.size(); }

  template<typename T>
  void integer( const T& val )
  {
    check_size( sizeof( T ) );
    memcpy( output_.data(), &val, sizeof( T ) );
    output_ = output_.subspan( sizeof( T ) );
  }

  void string( const std::string_view str )
  {
    check_size( str.size() );
    memcpy( output_.data(), str.data(), str.size() );
    output_ = output_.subspan( str.size() );
  }

  template<typename T>
  void object( const T& obj )
  {
    check_size( obj.serialized_length() );
    obj.serialize( *this );
  }
};
