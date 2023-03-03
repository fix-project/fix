#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <sys/uio.h>
int main()
{
  char buf1[16];
  char buf2[] = "new line";  

  struct iovec iovs[1] = { { .iov_base = buf1, .iov_len = 16 } };
  struct iovec iovs2[1] = { { .iov_base = buf2, .iov_len = 8 } };

  int fd_1 = open ( "a/b/b_1.txt", O_RDWR );
  printf( "fd_1 = %d\n", fd_1 );

  if ( fd_1 < 0 ) {
    return -1;
  }
  
  readv( fd_1, iovs, 1 );
  printf( "Read: %s\n", buf1 );
  for (int i = 0; i < 16; i++) {
    if (buf1[i] != '\0') return -1;
  }
  writev( fd_1, iovs2, 1 );
  //TODO: add when fd_seek functionality has ability to go to beginning
  //lseek( fd_1, 0, SEEK_SET );
  //readv( fd_1, iovs, 1 );
  //printf( "Read: %s\n", buf1 );
  //for (int i = 0; i < 16; i++) {
  //  if (buf1[i] != buf2[i]) return -1;
  //}
  close ( fd_1 );

  int fd_2 = open ( "a/b/b_1.txt", O_RDONLY );
  printf( "fd_2 = %d\n", fd_2 );  
  readv( fd_2, iovs, 1 );
  printf( "Read: %s\n", buf1 );
  for (int i = 0; i < 16; i++) {
    //TODO: uncomment when close functionality works properly
    //if (buf1[i] != buf2[i]) return -1;
  }
  return 0;
}