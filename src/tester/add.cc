#include "add.hh"
#include <cstring>
#include <stdlib.h>

int add( char* a, char* b )
{
  uint8_t x;
  uint8_t y;
  memcpy( &x, a, 1 );
  memcpy( &y, b, 1 );
  return x + y;
}

int Base::add( char* a, char* b )
{
  uint8_t x;
  uint8_t y;
  memcpy( &x, a, 1 );
  memcpy( &y, b, 1 );
  return x + y;
}

template<int N>
int Derived<N>::add( char* a, char* b )
{
  uint8_t x;
  uint8_t y;
  memcpy( &x, a, 1 );
  memcpy( &y, b, 1 );
  return x + y + N;
}

Base* make_derived()
{
  int x = rand();
  if ( x % 10 == 0 ) {
    return new Derived<0>( x );
  } else if ( x % 10 == 1 ) {
    return new Derived<1>( x );
  } else if ( x % 10 == 2 ) {
    return new Derived<2>( x );
  } else if ( x % 10 == 3 ) {
    return new Derived<3>( x );
  } else if ( x % 10 == 4 ) {
    return new Derived<4>( x );
  } else if ( x % 10 == 5 ) {
    return new Derived<5>( x );
  } else if ( x % 10 == 6 ) {
    return new Derived<6>( x );
  } else if ( x % 10 == 7 ) {
    return new Derived<7>( x );
  } else if ( x % 10 == 8 ) {
    return new Derived<8>( x );
  } else {
    return new Derived<9>( x );
  }
}
