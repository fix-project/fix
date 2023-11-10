#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main( int argc __attribute((unused)), char* argv[] )
{
  char* add_program_name = argv[1];
  int N = atoi( argv[2] );
  for ( volatile int i = 0; i < N; i++ ) {
    char const* arg1 = "a";
    char const* arg2 = "b";
    pid_t pid = vfork();
    int wstatus;
    if ( pid == 0 ) {
      execl( add_program_name, "add", arg1, arg2, NULL );
    } else {
      waitpid( pid, &wstatus, 0 );
    }
  }
}
