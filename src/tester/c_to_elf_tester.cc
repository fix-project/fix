#include <string>

#include "ccompiler.hh" 

using namespace std;

int main()
{
  c_to_elf( "test", "#include \"test.h\"", "" );
  return 0;
}
