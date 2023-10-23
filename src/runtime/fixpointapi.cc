#include <glog/logging.h>

#include "base64.hh"
#include "fixpointapi.hh"
#include "runtime.hh"
#include "wasm-rt.h"

namespace fixpoint {
void attach_tree( __m256i handle, wasm_rt_externref_table_t* target_table )
{
  GlobalScopeTimer<Timer::Category::AttachTree> record_timer;
  Handle tree_handle( handle );
  CHECK( tree_handle.is_tree() or tree_handle.is_tag() );
  CHECK( tree_handle.is_strict() );
  auto tree = Runtime::get_instance().storage().get_tree( tree_handle );
  target_table->ref = tree_handle;
  target_table->data = reinterpret_cast<wasm_rt_externref_t*>( const_cast<Handle*>( tree.data() ) );
  target_table->size = tree.size();
  target_table->max_size = tree.size();
}

void attach_blob( __m256i handle, wasm_rt_memory_t* target_memory )
{
  GlobalScopeTimer<Timer::Category::AttachBlob> record_timer;
  Handle blob_handle( handle );
  CHECK( blob_handle.is_blob() );
  CHECK( blob_handle.is_strict() );

  target_memory->ref = blob_handle;
  std::string_view blob;
  if ( blob_handle.is_literal_blob() ) {
    blob = { reinterpret_cast<const char*>( &target_memory->ref ), blob_handle.literal_blob_len() };
  } else {
    blob = Runtime::get_instance().storage().get_blob( blob_handle );
  }

  target_memory->data = reinterpret_cast<uint8_t*>( const_cast<char*>( blob.data() ) );
  target_memory->pages = ( blob.size() + WASM_RT_PAGE_SIZE - 1 ) / WASM_RT_PAGE_SIZE; // ceil(blob_size/page_size)
  target_memory->max_pages = target_memory->pages;
  target_memory->size = blob.size();
}

// module_instance points to the WASM instance
__m256i create_blob( wasm_rt_memory_t* memory, size_t size )
{
  GlobalScopeTimer<Timer::Category::CreateBlob> record_timer;
  if ( size > memory->size ) {
    wasm_rt_trap( WASM_RT_TRAP_OOB );
  }
  Blob_ptr data { reinterpret_cast<char*>( memory->data ) };
  memory->data = NULL;
  memory->pages = 0;
  memory->size = 0;
  return Runtime::get_instance().storage().add_blob( Blob( std::move( data ), size ) );
}

__m256i create_blob_i32( uint32_t content )
{
  GlobalScopeTimer<Timer::Category::CreateBlob> record_timer;
  return _mm256_set_epi32( 0x24'00'00'00, 0, 0, 0, 0, 0, 0, content );
}

__m256i create_tree( wasm_rt_externref_table_t* table, size_t size )
{
  GlobalScopeTimer<Timer::Category::CreateTree> record_timer;

  if ( size > table->size ) {
    wasm_rt_trap( WASM_RT_TRAP_OOB );
  }

  Tree_ptr data { reinterpret_cast<Handle*>( table->data ) };
  table->data = NULL;
  table->size = 0;
  return Runtime::get_instance().storage().add_tree( Tree( std::move( data ), size ) );
}

__m256i create_tag( __m256i handle, __m256i type )
{
  GlobalScopeTimer<Timer::Category::CreateTree> record_timer;

  Handle new_name = Runtime::get_instance().storage().add_tag( 3 );
  span_view<Handle> tag = Runtime::get_instance().storage().get_tree( new_name );

  tag.mutable_data()[0] = handle;
  tag.mutable_data()[1] = Runtime::get_instance().get_current_procedure();
  tag.mutable_data()[2] = type;

  return new_name;
}

__m256i create_thunk( __m256i handle )
{
  Handle encode( handle );
  return Handle::get_thunk_name( encode );
}

uint32_t get_value_type( __m256i handle )
{
  Handle object( handle );
  return static_cast<uint32_t>( object.get_content_type() );
}

uint32_t equality( __m256i lhs, __m256i rhs )
{
  Handle left( lhs );
  Handle right( rhs );
  return Runtime::get_instance().storage().compare_handles( left, right );
}

void unsafe_io( int32_t index, int32_t length, wasm_rt_memory_t* mem )
{
  if ( index + length > (int64_t)mem->size ) {
    wasm_rt_trap( WASM_RT_TRAP_OOB );
  }
  for ( int i = index; i < index + length; i++ ) {
    std::cout << mem->data[i];
  }
  std::cout << std::endl;
  std::flush( std::cout );
}

uint32_t get_length( __m256i handle )
{
  Handle h( handle );
  CHECK( not h.is_lazy() );
  return Handle( h ).get_length();
}

uint32_t get_access( __m256i handle )
{
  return static_cast<uint32_t>( Handle( handle ).get_laziness() );
}
__m256i lower( __m256i handle )
{
  Handle h( handle );
  switch ( h.get_laziness() ) {
    case Laziness::Lazy:
      return h;
    case Laziness::Shallow:
      return h.as_lazy();
    case Laziness::Strict:
      return h.as_shallow();
  }
  __builtin_unreachable();
}
}
