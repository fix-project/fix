#include <unistd.h>

void puts( const char* str )
{
  while ( *str )
    write( 1, str++, 1 );
}

int main()
{
  puts( "Hello, World!" );
  return 0;
}
