void fixpoint_exit( int a ) __attribute( ( import_name( "fixpoint_exit" ), import_module( "fixpoint-api" ) ) );

void proc_exit( int a ) __attribute( ( export_name( "proc_exit" ) ) );

void proc_exit( int a )
{
  fixpoint_exit( a );
}
