#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
  int fd = open ( "new_file.txt", O_CREAT );
  printf( "fd = %d\n", fd );
  close ( fd );
  return 0;
}