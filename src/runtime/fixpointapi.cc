#include "fixpointapi.hh"
#include "runtimestorage.hh"

namespace fixpoint {
// instance_size is the size of the WASM instance
void* init_module_instance( size_t instance_size, __m256i encode_name )
{
  // allocate aligned memory to hold FP instance and WASM instance
  size_t fixpoint_instance_size = sizeof( Instance ) + instance_size;
  if ( fixpoint_instance_size % alignof( Instance ) != 0 ) {
    fixpoint_instance_size = ( fixpoint_instance_size / alignof( Instance ) + 1 ) * alignof( Instance );
  }
  void* ptr = aligned_alloc( alignof( Instance ), fixpoint_instance_size );
  Instance* instance = new ( ptr ) Instance(encode_name );
  // advance to the end of the FP instance/beginning of the WASM instance, return a void* point to this spot
  instance++;

  return (void*)instance;
}

void* init_env_instance( size_t env_instance_size )
{
  return calloc( 1, env_instance_size );
}

// module_instance points to the WASM instance
void get_tree_entry( void* module_instance, uint32_t src_ro_handle, uint32_t entry_num, uint32_t target_ro_handle )
{
#if TIME_FIXPOINT_API
  RecordScopeTimer<Timer::Category::Nonblock> record_timer { _get_tree_entry };
#endif

  Instance* instance = (Instance*)module_instance - 1;

  ObjectReference* ro_handles = instance->get_ro_handles();

  const ObjectReference& obj = instance->get_ro_handle( src_ro_handle );

  if ( obj.name_.get_content_type() != ContentType::Tree ) {
    throw std::runtime_error( "not a tree" );
  }

  Name entry = RuntimeStorage::get_instance().get_tree( obj.name_ ).at( entry_num );

  ObjectReference ref( entry );
  ro_handles[target_ro_handle] = ref;
}

// module_instance points to the WASM instance
void attach_blob( void* module_instance, uint32_t ro_handle, wasm_rt_memory_t* target_memory )
{
#if TIME_FIXPOINT_API
  RecordScopeTimer<Timer::Category::Nonblock> record_timer { _attach_blob };
#endif
  //  attach blob at handle to target_memory
  Instance* instance = (Instance*)module_instance - 1;

  const ObjectReference& obj = instance->get_ro_handle( ro_handle );

  if ( obj.name_.get_content_type() != ContentType::Blob ) {
    throw std::runtime_error( "not a blob" );
  }

  std::string_view blob = RuntimeStorage::get_instance().get_blob( obj.name_ );

  target_memory->data = (uint8_t*)const_cast<char*>( blob.data() );
  target_memory->pages = blob.size() / getpagesize();
  target_memory->max_pages = blob.size() / getpagesize();
  target_memory->size = blob.size();
}

// module_instance points to the WASM instance
void detach_mem( void* module_instance, wasm_rt_memory_t* target_memory, uint32_t rw_handle )
{
#if TIME_FIXPOINT_API
  RecordScopeTimer<Timer::Category::Nonblock> record_timer { _detach_mem };
#endif
  if ( target_memory == NULL ) {
    throw std::runtime_error( "memory does not exist" );
  }
  Instance* instance = (Instance*)module_instance - 1;

  MutableValueReference* rw_handles = instance->get_rw_handles();

  MBlob* blob = new MBlob();
  MutableValueMeta meta;

  // void* ptr;
  // ptr = malloc( target_memory->size + sizeof( MutableValueMeta ) );
  // blob->setData( (uint8_t*)( (char*)ptr + sizeof( MutableValueMeta ) ) );
  // memcpy( blob->getData(), target_memory->data, target_memory->size );

  // wasm_rt_free_memory( target_memory );
  blob->set_data( target_memory->data );
  target_memory->data = NULL;
  target_memory->pages = 0;
  target_memory->max_pages = 65536;
  target_memory->size = 0;

  rw_handles[rw_handle] = blob;
}

// module_instance points to the WASM instance
void freeze_blob( void* module_instance, uint32_t rw_handle, size_t size, uint32_t ro_handle )
{
#if TIME_FIXPOINT_API
  RecordScopeTimer<Timer::Category::Nonblock> record_timer { _freeze_blob };
#endif
  Instance* instance = (Instance*)module_instance - 1;

  MutableValueReference* rw_handles = instance->get_rw_handles();

  MutableValueReference ref = rw_handles[rw_handle];

  // TODO: make sure the mutablevalue that mutablevaluereference points to is a mblob, or else trap

  std::string blob_content( (char*)ref->get_data(), size );

  Name blob = RuntimeStorage::get_instance().add_blob( std::move( blob_content ) );

  ObjectReference obj( blob );

  ObjectReference* ro_handles = instance->get_ro_handles();

  ro_handles[ro_handle] = obj;
}

// module_instance points to the WASM instance
void designate_output( void* module_instance, uint32_t ro_handle )
{
#if TIME_FIXPOINT_API
  RecordScopeTimer<Timer::Category::Nonblock> record_timer { _designate_output };
#endif
  Instance* instance = (Instance*)module_instance - 1;

  const ObjectReference& obj = instance->get_ro_handle( ro_handle );

  instance->set_output( obj.name_ );
}
}
