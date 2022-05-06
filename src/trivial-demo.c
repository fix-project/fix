void fixpoint_exit( int x ) __attribute((import_name("fixpoint_exit"), import_module("fixpoint-api")));

void proc_exit( int x ) __attribute((export_name("proc_exit")))
{
  fixpoint_exit( x );
}
