#include <string>
#include <iostream>

#include "ccompiler.hh" 

using namespace std;

int main()
{
  c_to_elf( "test", "#include \"test.h\"\n int direct( int a ) { return a; }", "int direct( int a );" );
  return 0;
}
