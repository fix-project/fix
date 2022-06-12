#include <stdint.h>

int add( char* a, char* b );

class Base
{
public:
  int a_;
  virtual int add( char* a, char* b );
  virtual ~Base() = default;
  Base( int a )
    : a_( a )
  {
  }
};

template<int N>
class Derived : public Base
{
public:
  int add( char* a, char* b ) override;
  Derived( int a )
    : Base( a )
  {
  }
};

template class Derived<0>;
template class Derived<1>;
template class Derived<2>;
template class Derived<3>;
template class Derived<4>;
template class Derived<5>;
template class Derived<6>;
template class Derived<7>;
template class Derived<8>;
template class Derived<9>;

Base* make_derived();
