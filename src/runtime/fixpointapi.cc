#include "fixpointapi.hh"
#include "mutablevalue.hh"
#include "objectreference.hh"
#include "runtimestorage.hh"

namespace fixpoint {
void attach_tree( __m256i ro_handle, wasm_rt_externref_table_t* target_table )
{
  GlobalScopeTimer<Timer::Category::AttachTree> record_timer;
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
  GlobalScopeTimer<Timer::Category::AttachBlob> record_timer;
  ObjectReference obj( ro_handle );
  if ( obj.get_content_type() != ContentType::Blob || !obj.is_accessible() ) {
    throw std::runtime_error( "not an accssible blob" );
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
  return RuntimeStorage::get_instance().add_local_blob( Blob( move( data ), size ) );
}

__m256i create_blob_i32( uint32_t content )
{
  GlobalScopeTimer<Timer::Category::CreateBlob> record_timer;
  return RuntimeStorage::get_instance().add_local_blob( make_blob( content ) );
}

__m256i create_tree( wasm_rt_externref_table_t* table, size_t size )
{
  GlobalScopeTimer<Timer::Category::CreateTree> record_timer;

  if ( size > table->size ) {
    wasm_rt_trap( WASM_RT_TRAP_OOB );
  }

  for ( size_t i = 0; i < size; i++ ) {
    table->data[i] = MTreeEntry::to_name( MTreeEntry( table->data[i] ) );
  }
  Tree_ptr data { reinterpret_cast<Name*>( table->data ) };
  table->data = NULL;
  table->size = 0;
  return RuntimeStorage::get_instance().add_local_tree( Tree( move( data ), size ) );
}

__m256i create_thunk( __m256i ro_handle )
{
  Name encode( ro_handle );
  return Name::get_thunk_name( encode );
}
}
