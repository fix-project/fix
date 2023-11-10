#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main( int argc __attribute( ( unused ) ), char* argv[] )
{
  uint8_t a;
  uint8_t b;
  memcpy( &a, argv[1], 1 );
  memcpy( &b, argv[2], 1 );
  return a + b;
}
