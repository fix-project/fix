
extern "C" {
#include "../util/fixpoint_util.h"
}
#include <string>

const int ASYNCIFY_MEMORY_LENGTH = 100;

bool sleeping = false;
int counter = 0;
int32_t* asyncify_memory;

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

void start_unwind( int32_t* asyncify_memory )
  __attribute__( ( import_module( "asyncify" ), import_name( "start_unwind" ) ) );
void stop_rewind() __attribute__( ( import_module( "asyncify" ), import_name( "stop_rewind" ) ) );
int32_t get_wasm_memory_offset( int32_t* pointer )
  __attribute__( ( import_module( "asyncify_storage" ), import_name( "get_memory_offset" ) ) );

void unsafe_io( std::string s )
{
  fixpoint_unsafe_io( s.c_str(), s.length() );
}

int32_t* get_asyncify_memory() __attribute__( ( export_name( "asyncify_memory_offset" ) ) )
{
  return asyncify_memory;
}

void fail()
{
  create_tree_rw_table_0( 10 ); // oob
}

void check_counter( int32_t expected_val )
{
  if ( counter != expected_val ) {
    unsafe_io( "counter value incorrect. expected value " + std::to_string( expected_val ) + " actual value "
               + std::to_string( counter ) );
    fail();
  }
}

// allocate and initialize asyncify data.
// per asyncify, first entry is a pointer to start of asyncify's memory
// second entry is a pointer to end of asyncify's memory
// remainder of buffer is asyncify's memory
void initialize_asyncify_data()
{
  asyncify_memory = (int32_t*)malloc( ASYNCIFY_MEMORY_LENGTH );
  if ( asyncify_memory == NULL ) {
    unsafe_io( "could not allocate memory for asyncify" );
    fail();
  }
  asyncify_memory[0] = get_wasm_memory_offset( &asyncify_memory[2] );
  asyncify_memory[1] = get_wasm_memory_offset( &asyncify_memory[ASYNCIFY_MEMORY_LENGTH - 1] );
}

// This function does not get asyncified (and thus every line is always executed)
// because it calls start_unwind.
void sleep()
{
  counter++;
  if ( !sleeping ) {
    counter++;
    check_counter( 4 );
    sleeping = true;
    initialize_asyncify_data();
    start_unwind( asyncify_memory );
  } else {
    stop_rewind();
    counter++;
    check_counter( 6 );
  }
}

void test()
{
  counter++;
  check_counter( 2 );
  sleep();
  counter++;
  check_counter( 7 );
}

externref apply( externref encode ) __attribute__( ( export_name( "apply" ) ) )
{
  externref blob = create_blob_i32( 1 );
  counter++;
  check_counter( 1 );
  test();
  counter++;
  check_counter( 8 );
  return create_blob_i32( 1 );
  //   return blob; // TODO(Katie) this doesn't work. Probably an error with asyncify externrefs.
}