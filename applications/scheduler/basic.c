#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/fixpoint_util.h"

int32_t get_total_size( externref handle )
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_total_size" ) ) );

#define FIXPOINT_APPLY __attribute__( ( export_name( "_fixpoint_apply" ) ) )

// Assumes encode element 0 is the list of computation nodes
// That element has the structure:
// [
//    {
//      cores: int,
//      link speed: int,
//    },
//    {
//      cores: int,
//      link speed: int,
//    },
// ]

struct node
{
  unsigned index;
  unsigned cores;
  unsigned speed; // link speed (0 == instant)
};

uint32_t compute_time( struct node node, size_t size )
{
  // (Very Bad) Assumptions:
  // - data to receive is one-tenth the size of the data to transmit
  // - computation will take 256 seconds on a single-core machine, and will parallelize perfectly
  //
  // These should be replaced by metadata in the ENCODE.
  float transmit_time = node.speed == 0 ? 0 : size / (float)node.speed;
  float compute_time = 256.f / node.cores;
  float receive_time = transmit_time / 10.f;
  uint32_t total_time = transmit_time + compute_time + receive_time;
  return total_time;
}

FIXPOINT_APPLY externref _fixpoint_apply( externref encode )
{
  attach_tree_ro_table_0( encode );
  externref nodes = get_ro_table_0( 2 );
  attach_tree_ro_table_1( nodes );
  int32_t n = size_ro_table_1();

  struct node computers[n];
  for ( size_t i = 0; i < n; i++ ) {
    attach_tree_ro_table_2( get_ro_table_1( i ) );
    attach_blob_ro_mem_0( get_ro_table_2( 0 ) );
    attach_blob_ro_mem_1( get_ro_table_2( 1 ) );
    computers[i].index = i;
    computers[i].cores = get_i32_ro_mem_0( 0 );
    computers[i].speed = get_i32_ro_mem_1( 0 );
  }

  uint32_t times[n];
  for ( size_t i = 0; i < n; i++ ) {
    times[i] = compute_time( computers[i], get_total_size( get_ro_table_0(3) ) );
  }

  size_t best_node = 0;
  for ( size_t i = 1; i < n; i++ ) {
    if ( times[i] < times[best_node] ) {
      best_node = i;
    }
  }

  return create_blob_i32( best_node );
}
