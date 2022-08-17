#include <fcntl.h>
#include <stdio.h>
#include <sys/uio.h>

int main()
{
  char buf1[3];
  char buf2[3];

  struct iovec iovs[2] = { { .iov_base = buf1, .iov_len = 2 }, { .iov_base = buf2, .iov_len = 2 } };

  int fd = open( "a/b/c/fixpoint", O_RDONLY );

  readv( fd, iovs, 2 );

  printf( "Read %s and %s\n", buf1, buf2 );
 
  int fd_2 = open( "a/b/hello/greeter.txt", O_RDONLY );
  
  printf( "fd_2 = %d", fd_2 );

  int fd_3 = open( "a/b/i-dont-exist", O_RDONLY );

  printf( "fd_3 = %d", fd_3 );

  // fd_3 should be negative, since it shouldn't be able to find the file
  if ( fd_3 >= 0 ) { 
    return -1;
  }

  return 0;
}
