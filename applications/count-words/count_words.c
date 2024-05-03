#include "fixpoint_util.h"
#include "support.h"

#include <stdlib.h>
#include <string.h>

void out(const char *s) {
  fixpoint_unsafe_io(s, strlen(s));
}

int compare( const void* a, const void* b )
{
  char *const *s = a;
  char *const *t = b;
  return strcmp( *s, *t );
}

void swap(char **a, char **b) {
  char *temp = *a;
  *a = *b;
  *b = temp;
}

void quicksort(char *arr[], unsigned int length) {
  unsigned piv = 0;
  if (length <= 1) 
    return;
  for (unsigned i = 0; i < length; i++) {
    if (strcmp(arr[i], arr[length -1]) < 0)
      swap(arr + i, arr + piv++);		
  }
  swap(arr + piv, arr + length - 1);
  quicksort(arr, piv++);
  quicksort(arr + piv, length - piv);
}

void heapify(char *arr[], unsigned length, int i) {
  int largest = i;
  int left = 2*i+1;
  int right = 2*i+2;
  if (left < length && strcmp(arr[left], arr[largest]) > 1)
    largest = left;
  if (right < length && strcmp(arr[right], arr[largest]) > 1)
    largest = right;
  if (largest != i) {
    swap(&arr[i], &arr[largest]);
    __attribute__((musttail)) return heapify(arr, length, largest);
  }
}

void hsort(char *arr[], unsigned length) {
  for (int i = length / 2 - 1; i >= 0; i--)
    heapify(arr, length, i);

  for (int i = length - 1; i > 0; i--) {
    swap(&arr[0], &arr[i]);
    heapify(arr, i, 0);
  }
}

/**
 * @brief Counts the words in the input blob.
 * @return externref  A CSV file as a blob.
 */
__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref combination )

{
  out("count words: starting");
  externref nil = create_blob_rw_mem_0( 0 );

  attach_tree_ro_table_0( combination );

  /* auto rlimits = get_ro_table_0( 0 ); */
  /* auto self = get_ro_table_0( 1 ); */
  externref input = get_ro_table_0( 2 );

  attach_blob_ro_mem_0( input );
  size_t size = get_length( input );
  char* file = (char*)malloc( size );
  ro_mem_0_to_program_mem( file, 0, size );
  const char* delim = " \n\t,.!?;";

  size_t num = 1;
  for ( size_t i = 0; i < size; i++ )
    num += strchr( delim, file[i] ) ? 1 : 0;

  char** strings = (char**)malloc( sizeof( char* ) * num );

  out("count words: tokenizing words");
  size_t N = 0;
  char *p = file;
  while (*p) {
    char *q = p;
    bool valid = true;
    while (*p && !strchr(delim, *p)) {
      if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z'))) {
        valid = false;
      }
      p++;
    }
    while (*p && strchr(delim, *p)) {
      *p = 0;
      p++;
    }
    if (valid)
      strings[N++] = q;
  }

  if (N > num) {
    out("count words: size error");
  }

  out("count words: sorting words");
  /* qsort( strings, N, sizeof( char* ), compare ); */
  /* quicksort( strings, N); */
  hsort(strings, N);
  out("count words: done sorting words");

  char** keys = (char**)malloc( sizeof( char* ) * N );
  size_t* values = (size_t*)malloc( sizeof( size_t ) * N );
  size_t i = 0;

  out("count words: counting words");
  size_t char_count = 0;
  for ( size_t j = 0; j < N; ) {
    char* current = strings[j];
    char_count += strlen( current );
    size_t k = j;
    while ( k < N && !strcmp( strings[k], current ) )
      k++;
    size_t n = k - j;
    keys[i] = current;
    values[i] = n;
    i++;
    j = k;
  }
  size_t n = i;

  char buf[10];
  i = 0;
  for ( size_t j = 0; j < 32; j += 4 ) {
    size_t shift = ( 28 - j );
    int x = (char_count >> shift) & 0xf;
    char y = "0123456789abcdef"[x];
    buf[i++] = y;
  }
  fixpoint_unsafe_io(buf, 8);

  // number of characters in words, plus ",0x%08x\n" on every line
  size_t output_bytes = char_count + n * ( 1 + 10 + 1 );

  char* output = (char*)malloc( output_bytes );
  if (!output) {
    out("count words: could not allocate output buffer");
  }

  char* cursor = output;
  out("count words: starting output generation");
  for ( size_t i = 0; i < n; i++ ) {
    size_t m = strlen( keys[i] );
    strncpy( cursor, keys[i], m );
    cursor += m;
    *cursor++ = ',';
    *cursor++ = '0';
    *cursor++ = 'x';
    for ( size_t j = 0; j < 32; j += 4 ) {
      size_t shift = ( 28 - j );
      int x = values[i] >> shift;
      char y = "0123456789abcdef"[x];
      *cursor++ = y;
    }
    *cursor++ = '\n';
  }

  size_t pages = output_bytes / 4096 + 1;
  if (grow_rw_mem_0_pages( pages ) == -1) {
    char *s = "count words: grow error";
    fixpoint_unsafe_io(s, strlen(s));
  }
  program_mem_to_rw_mem_0( 0, output, output_bytes );

  return create_blob_rw_mem_0( output_bytes );
}
