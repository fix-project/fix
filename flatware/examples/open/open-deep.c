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

  printf( "Read %s and %s", buf1, buf2 );
  return 0;
}
