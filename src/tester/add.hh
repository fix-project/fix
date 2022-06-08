#include <stdint.h>

int add( int a, int b );
int add( const char* a, const char* b );

class Base
{
public:
  int a, b;
  virtual int add( int x, int y );
  virtual int add_char( const char* x, const char* y );
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
  int add_char( const char* x, const char* y ) override;
  Derived( int a_, int b_ )
    : Base( a_, b_ )
  {
  }
};
