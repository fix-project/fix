#include <stdio.h>

int main()
{
  int result;
  FILE* fd = fopen( "saskghasfg", "r" );
  if ( fd != NULL ) {
    return 78;
  } else {
    return 10086;
  }
  // fread( &result, 1, 1, fd );
}
