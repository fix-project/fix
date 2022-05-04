#include "fixpointapi.hh"
#include "runtimestorage.hh"
#include "runtimevalue.hh"

namespace fixpoint {
// instance_size is the size of the WASM instance
void* init_module_instance( size_t instance_size )
{
  return calloc( 1, instance_size );
}

void* init_env_instance( size_t env_instance_size )
{
  void* ptr = aligned_alloc( alignof( __m256i ), env_instance_size );
  memset( ptr, 0, env_instance_size );
  return ptr;
}

void free_env_instance( void* env_instance )
{
  free( env_instance );
}

void attach_tree( __m256i ro_handle, wasm_rt_externref_table_t* target_table )
{
  ObjectReference obj( ro_handle );
  if ( obj.get_content_type() != ContentType::Tree || !obj.is_accessible() ) {
    throw std::runtime_error( "not an accessible tree" );
  }
  auto tree = RuntimeStorage::get_instance().get_tree( ObjectReference::object_reference_name_only( obj ) );
  target_table->data = reinterpret_cast<wasm_rt_externref_t*>( const_cast<Name*>( tree.data() ) );
  target_table->size = tree.size();
  target_table->max_size = tree.size();
}

void attach_blob( __m256i ro_handle, wasm_rt_memory_t* target_memory )
{
#if TIME_FIXPOINT_API
  RecordScopeTimer<Timer::Category::Nonblock> record_timer { _attach_blob };
#endif
  ObjectReference obj( ro_handle );
  if ( !obj.is_accessible() ) {
    throw std::runtime_error( "object handle is not accessible" );
  }
  if ( obj.get_content_type() != ContentType::Blob ) {
    throw std::runtime_error( "not a blob" );
  }

  std::string_view blob;
  if ( obj.is_literal_blob() ) {
    target_memory->ref = ro_handle;
    blob = { reinterpret_cast<const char*>( &target_memory->ref ), obj.literal_blob_len() };
  } else {
    blob = RuntimeStorage::get_instance().get_blob( (__m256i)obj );
  }

  target_memory->data = (uint8_t*)const_cast<char*>( blob.data() );
  target_memory->pages = blob.size() / getpagesize();
  target_memory->max_pages = blob.size() / getpagesize();
  target_memory->size = blob.size();
}

__m256i detach_mem( wasm_rt_memory_t* target_memory )
{
#if TIME_FIXPOINT_API
  RecordScopeTimer<Timer::Category::Nonblock> record_timer { _detach_mem };
#endif
  MBlob* blob = new MBlob();
  *blob = *target_memory;
  target_memory->data = NULL;
  target_memory->pages = 0;
  target_memory->max_pages = 65536;
  target_memory->size = 0;

  MutableValueReference ref( blob, true );
  return ref;
}

// module_instance points to the WASM instance
__m256i freeze_blob( __m256i rw_handle, size_t size )
{
#if TIME_FIXPOINT_API
  RecordScopeTimer<Timer::Category::Nonblock> record_timer { _freeze_blob };
#endif
  // TODO: make sure the mutablevalue that mutablevaluereference points to is a mblob, or else trap
  MutableValueReference ref( rw_handle );
  std::string blob_content( (char*)ref.get_mblob_ptr()->data, size );

  wasm_rt_free_memory_sw_checked( ref.get_mblob_ptr() );
  delete ( ref.get_mblob_ptr() );

  Name blob = RuntimeStorage::get_instance().add_blob( std::move( blob_content ) );

  ObjectReference obj( blob );
  return obj;
}
}
