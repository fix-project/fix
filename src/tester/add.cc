#include "add.hh"
#include <cstring>

int add( int a, int b )
{
  return a + b;
}

inline uint32_t cast( char buffer[4] )
{
  return (uint32_t)buffer[0] << 24 | (uint32_t)buffer[1] << 16 | (uint32_t)buffer[2] << 8 | (uint32_t)buffer[3];
}

uint32_t add( char a[4], char b[4] )
{
  return cast( a ) + cast( b );
}

int Base::add( int x, int y )
{
  return x + y;
}

uint32_t Base::add_char( char x[4], char y[4] )
{
  return cast( x ) + cast( y );
}

int Derived::add( int x, int y )
{
  return x + y;
}

uint32_t Derived::add_char( char x[4], char y[4] )
{
  return cast( x ) + cast( y );
}
