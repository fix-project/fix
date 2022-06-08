#include <stdint.h>

int add( int a, int b );
uint32_t add( char a[4], char b[4] );

class Base
{
public:
  int a, b;
  virtual int add( int x, int y );
  virtual uint32_t add_char( char x[4], char y[4] );
  virtual ~Base() = default;
  Base( int a_, int b_ )
    : a( a_ )
    , b( b_ )
  {
  }
};

class Derived : public Base
{
public:
  int add( int x, int y ) override;
  uint32_t add_char( char x[4], char y[4] ) override;
  Derived( int a_, int b_ )
    : Base( a_, b_ )
  {
  }
};
