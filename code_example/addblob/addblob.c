extern int path_open( char name[] ) __attribute__((export_name("path_open")));
extern int fd_read( int fd_id, int offset, int size );
extern int fd_write( int fd_id, int offset, int size );

void _start( int a, int b ) __attribute__((export_name("_start")))
{
  int fd = path_open( "output" );
  int c = a + b;
  fd_write( fd, c, sizeof( int ) );
}
