#include "fixpoint_util.h"
#include "support.h"

#include <stdlib.h>
#include <string.h>

char decode( char x )
{
  if ( x >= '0' && x <= '9' ) {
    return x - '0';
  }
  if ( x >= 'a' && x <= 'f' ) {
    return x - 'a';
  }
  if ( x >= 'A' && x <= 'F' ) {
    return x - 'A';
  }
  return 0;
}

/**
 * @brief Merges two wordcount CSVs into a single larger one.
 * @return externref  A wordcount CSV.
 */
__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref combination )
{
  externref nil = create_blob_rw_mem_0( 0 );

  attach_tree_ro_table_0( combination );

  /* auto rlimits = get_ro_table_0( 0 ); */
  /* auto self = get_ro_table_0( 1 ); */
  externref X = get_ro_table_0( 2 );
  externref Y = get_ro_table_0( 3 );

  attach_blob_ro_mem_0( X );
  attach_blob_ro_mem_1( Y );
  size_t sX = get_length( X );
  size_t sY = get_length( Y );

  char* fX = (char*)malloc( sX );
  char* fY = (char*)malloc( sY );
  ro_mem_0_to_program_mem( fX, 0, sX );
  ro_mem_1_to_program_mem( fY, 0, sY );

  size_t nX = 0;
  size_t nY = 0;

  for ( size_t i = 0; i < sX; i++ )
    nX += fX[i] == '\n';
  for ( size_t i = 0; i < sY; i++ )
    nY += fY[i] == '\n';

  char** keysX = (char**)malloc( sizeof( char* ) * nX );
  char** valsXstr = (char**)malloc( sizeof( char* ) * nX );
  char** keysY = (char**)malloc( sizeof( char* ) * nY );
  char** valsYstr = (char**)malloc( sizeof( char* ) * nY );

  size_t NX = 0;
  char* p = strtok( fX, "," );
  do {
    char* q = strtok( NULL, "\n" );
    if ( !q )
      break;
    keysX[NX] = p;
    valsXstr[NX] = q;
    NX++;
  } while ( ( p = strtok( NULL, "," ) ) );

  size_t NY = 0;
  p = strtok( fY, "," );
  do {
    char* q = strtok( NULL, "\n" );
    if ( !q )
      break;
    keysY[NY] = p;
    valsYstr[NY] = q;
    NY++;
  } while ( ( p = strtok( NULL, "," ) ) );

  size_t* valsX = (size_t*)malloc( sizeof( size_t ) * NX );
  size_t* valsY = (size_t*)malloc( sizeof( size_t ) * NY );

  for ( size_t i = 0; i < NX; i++ ) {
    char* s = valsXstr[i];
    uint32_t x = 0;
    for ( size_t j = 0; j < 8; j++ ) {
      char c = s[2 + j];
      x |= decode( c ) << ( 7 - j ) * 4;
    }
    valsX[i] = x;
  }
  for ( size_t i = 0; i < NY; i++ ) {
    char* s = valsYstr[i];
    uint32_t x = 0;
    for ( size_t j = 0; j < 8; j++ ) {
      char c = s[2 + j];
      x |= decode( c ) << ( 7 - j ) * 4;
    }
    valsY[i] = x;
  }

  size_t N = NX + NY;
  char** keys = (char**)malloc( sizeof( char* ) * N );
  size_t* vals = (size_t*)malloc( sizeof( size_t ) * N );
  size_t i = 0;
  size_t ix = 0;
  size_t iy = 0;

  while ( ix < NX || iy < NY ) {
    if ( ix >= NX ) {
      keys[i] = keysY[iy];
      vals[i] = valsY[iy];
      i++;
      iy++;
      continue;
    } else if ( iy >= NY ) {
      keys[i] = keysX[ix];
      vals[i] = valsX[ix];
      i++;
      ix++;
      continue;
    }

    int cmp = strcmp( keysX[ix], keysY[iy] );

    if ( cmp < 0 ) {
      keys[i] = keysX[ix];
      vals[i] = valsX[ix];
      i++;
      ix++;
    } else if ( cmp == 0 ) {
      keys[i] = keysX[ix];
      vals[i] = valsX[ix] + valsY[iy];
      i++;
      ix++;
      iy++;
    } else {
      keys[i] = keysY[iy];
      vals[i] = valsY[iy];
      i++;
      iy++;
    }
  }

  size_t n = i;
  size_t char_count = 0;
  for ( size_t i = 0; i < n; i++ ) {
    char* current = keys[i];
    char_count += strlen( current );
  }

  size_t output_bytes = char_count + n * ( 1 + 10 + 1 );
  char* output = (char*)malloc( output_bytes );

  char* cursor = output;
  for ( size_t i = 0; i < n; i++ ) {
    size_t m = strlen( keys[i] );
    strcpy( cursor, keys[i] );
    cursor += m;
    *cursor++ = ',';
    *cursor++ = '0';
    *cursor++ = 'x';
    for ( size_t j = 0; j < 32; j += 4 ) {
      size_t shift = ( 28 - j );
      int x = vals[i] >> shift;
      char y = "0123456789abcdef"[x];
      *cursor++ = y;
    }
    *cursor++ = '\n';
  }

  size_t pages = output_bytes / 4096 + 1;
  grow_rw_mem_0_pages( pages );
  program_mem_to_rw_mem_0( 0, output, output_bytes );

  return create_blob_rw_mem_0( output_bytes );
}
