#include "add.hh"
#include <cstring>
#include <stdlib.h>

int add( int a, int b )
{
  return a + b;
}

int add( const char* a, const char* b )
{
  return atoi( a ) + atoi( b );
}

int Base::add( int x, int y )
{
  return x + y;
}

int Base::add_char( const char* x, const char* y )
{
  return atoi( x ) + atoi( y );
}

int Derived::add( int x, int y )
{
  return x + y;
}

int Derived::add_char( const char* x, const char* y )
{
  return atoi( x ) + atoi( y );
}
