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

// module_instance points to the WASM instance
__m256i create_blob( wasm_rt_memory_t* memory, size_t size )
{
  GlobalScopeTimer<Timer::Category::CreateBlob> record_timer;
  Name blob = RuntimeStorage::get_instance().add_local_blob( Blob( (char*)memory->data, size ) );
  memory->data = NULL;
  memory->pages = 0;
  memory->max_pages = 65536;
  memory->size = 0;

  ObjectReference obj( blob );
  return obj;
}

__m256i create_tree( wasm_rt_externref_table_t* table, size_t size )
{
  GlobalScopeTimer<Timer::Category::CreateTree> record_timer;
  for ( size_t i = 0; i < size; i++ ) {
    table->data[i] = MTreeEntry::to_name( MTreeEntry( table->data[i] ) );
  }
  Tree tree( reinterpret_cast<Name*>( table->data ), size );
  Name tree_name = RuntimeStorage::get_instance().add_local_tree( std::move( tree ) );
  table->data = NULL;
  table->size = 0;

  ObjectReference obj( tree_name );
  return obj;
}

__m256i create_thunk( __m256i ro_handle )
{
  Name encode( ro_handle );
  return Name::get_thunk_name( encode );
}
}
