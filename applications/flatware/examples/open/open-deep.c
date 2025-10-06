#include <fcntl.h>
#include <stdio.h>
#include <sys/uio.h>

int main()
{
  char buf1[3];
  char buf2[3];

  struct iovec iovs[2] = { { .iov_base = buf1, .iov_len = 2 }, { .iov_base = buf2, .iov_len = 2 } };

  // open and read a file

  int fd = open( "a/b/b_1.txt", O_RDONLY );

  printf( "fd = %d\n", fd );

  if ( fd < 0 ) {
    return -1;
  }
  readv( fd, iovs, 2 );

  printf( "Read %s and %s\n", buf1, buf2 );

  // open a second file in same program

  int fd_2 = open( "a/b/hello/greeter.txt", O_RDONLY );

  printf( "fd_2 = %d\n", fd_2 );

  if ( fd_2 < 0 ) {
    return -1;
  }
  readv( fd_2, iovs, 2 );

  printf( "Read %s and %s\n", buf1, buf2 );

  // open a directory (not a file)

  int fd_3 = open( "a/b/", O_RDONLY );

  printf( "fd_3 = %d\n", fd_3 );

  if ( fd_3 < 0 ) {
    return -1;
  }

  // try to open a file that does not exist

  int fd_4 = open( "a/b/i-dont-exist.txt", O_RDONLY );

  printf( "fd_4 = %d\n", fd_4 );

  if ( fd_4 >= 0 ) {
    return -1;
  }

  // try to open a directory that does not exist
  int fd_5 = open( "a/b/i-dont-exist", O_RDONLY );

  printf( "fd_5 = %d\n", fd_5 );

  if ( fd_5 >= 0 ) {
    return -1;
  }

  // try to open a file with a prefix that exists
  int fd_6 = open( "a/a_1.txt.txt", O_RDONLY );

  printf( "fd_6 = %d\n", fd_6 );

  if ( fd_6 >= 0 ) {
    return -1;
  }

  // see if case sensitive
  int fd_7 = open( "a/A_1.txt", O_RDONLY );

  printf( "fd_7 = %d\n", fd_7 );

  if ( fd_7 >= 0 ) {
    return -1;
  }

  return 0;
}
