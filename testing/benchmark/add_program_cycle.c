#include <stdio.h>
#include <stdlib.h>

int main( int argc, char* argv[] )
{
  char* add_program_name = argv[1];
  int N = atoi( argv[2] );
  for ( int i = 0; i < N; i++ ) {
    char const* arg1 = "42";
    char const* arg2 = "10086";
    pid_t pid = vfork();
    int wstatus;
    if ( pid == 0 ) {
      execl( add_program_name, "add", arg1, arg2, NULL );
    } else {
      waitpid( pid, &wstatus, 0 );
    }
  }
}
