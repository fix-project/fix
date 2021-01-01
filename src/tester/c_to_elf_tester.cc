#include <string>
#include <iostream>
#include <fstream>

#include "ccompiler.hh" 

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc != 2 )
  {
    cerr << "Usage: " << argv[0] << " output_directory\n";
    return 1;
  } 

  string res = c_to_elf( "test", "#include \"test.h\"\n int add( int a, int b ) { return a + b; }\n int direct( int a ) { return add( a, a) ; }", "#include <string.h>\n int direct( int a );" );
  
  ofstream fout;
  fout.open( argv[1], ios_base::out | ios_base::binary | ios_base::trunc );
  fout << res;
  fout.close();
  
  return 0;
}
