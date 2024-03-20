#include <glog/logging.h>
#include <stdexcept>

#include "fixpointapi.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "object.hh"
#include "runtimestorage.hh"
#include "wasm-rt.h"

using namespace std;

template<typename T>
void check( optional<T> t )
{
  if ( t ) {
    return;
  } else {
    throw std::runtime_error( "Invalid handle" );
  }
}

namespace fixpoint {
void attach_tree( u8x32 handle, wasm_rt_externref_table_t* target_table )
{
  auto fix_handle = Handle<Fix>::forge( handle );
  optional<Handle<AnyTree>> h
    = handle::extract<ExpressionTree>( fix_handle )
        .transform( []( auto h ) -> Handle<AnyTree> { return h; } )
        .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ObjectTree>( fix_handle ); } )
        .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ValueTree>( fix_handle ); } );

  check( h );

  auto tree = storage->get( *h );
  target_table->ref = h->content;
  target_table->data = reinterpret_cast<wasm_rt_externref_t*>( const_cast<Handle<Fix>*>( tree->data() ) );
  target_table->size = tree->size();
  target_table->max_size = tree->size();
}

void attach_blob( u8x32 handle, wasm_rt_memory_t* target_memory )
{
  auto h = handle::extract<Blob>( Handle<Fix>::forge( handle ) );
  check( h );

  target_memory->ref = h->content;

  BlobSpan blob = h->try_into<Named>()
                    .transform( [&]( auto h ) { return storage->get( h )->span(); } )
                    .or_else( [&]() -> optional<BlobSpan> {
                      return span<const char> { reinterpret_cast<const char*>( &target_memory->ref ),
                                                h->try_into<Literal>()->size() };
                    } )
                    .value();

  target_memory->data = reinterpret_cast<uint8_t*>( const_cast<char*>( blob.data() ) );
  target_memory->pages = ( blob.size() + WASM_RT_PAGE_SIZE - 1 ) / WASM_RT_PAGE_SIZE; // ceil(blob_size/page_size)
  target_memory->max_pages = target_memory->pages;
  target_memory->size = blob.size();
}

// module_instance points to the WASM instance
u8x32 create_blob( wasm_rt_memory_t* memory, size_t size )
{
  if ( size > memory->size ) {
    wasm_rt_trap( WASM_RT_TRAP_OOB );
  }

  auto blob = OwnedBlob(
    {
      reinterpret_cast<char*>( memory->data ),
      size,
    },
    AllocationType::Allocated );
  memory->data = NULL;
  memory->pages = 0;
  memory->size = 0;
  return storage->create( make_shared<OwnedBlob>( move( blob ) ) ).into<Fix>().content;
}

u8x32 create_blob_i32( uint32_t content )
{
  return Handle<Literal>( content ).into<Fix>().content;
}

u8x32 create_blob_i64( uint64_t content )
{
  return Handle<Literal>( content ).into<Fix>().content;
}

u8x32 create_blob_string( uint32_t index, uint32_t length, wasm_rt_memory_t* memory )
{
  if ( index + length > (int64_t)memory->size ) {
    wasm_rt_trap( WASM_RT_TRAP_OOB );
  }

  return storage->create( { reinterpret_cast<char*>( memory->data ) + index, length } ).into<Fix>().content;
}

void unsafe_io( int32_t index, int32_t length, wasm_rt_memory_t* mem )
{
  if ( index + length > (int64_t)mem->size ) {
    wasm_rt_trap( WASM_RT_TRAP_OOB );
  }
  for ( int i = index; i < index + length; i++ ) {
    cout << mem->data[i];
  }
  cout << endl;
  flush( cout );
}

u8x32 create_tree( wasm_rt_externref_table_t* table, size_t size )
{
  if ( size > table->size ) {
    wasm_rt_trap( WASM_RT_TRAP_OOB );
  }

  auto tree = OwnedTree(
    {
      reinterpret_cast<Handle<Fix>*>( table->data ),
      size,
    },
    AllocationType::Allocated );
  table->data = NULL;
  table->size = 0;

  return storage->create( make_shared<OwnedTree>( move( tree ) ) )
    .visit<Handle<Fix>>( []( auto h ) { return h; } )
    .content;
}

u8x32 create_tag( u8x32 handle, u8x32 type )
{
  auto tree = OwnedMutTree::allocate( 3 );

  tree[0] = current_procedure;
  tree[1] = Handle<Fix>::forge( handle );
  tree[2] = Handle<Fix>::forge( type );

  auto tree_handle = storage->create( make_shared<OwnedTree>( std::move( tree ) ) );

  return tree_handle.visit<Handle<Fix>>( []( auto h ) { return h.tag(); } ).content;
}

u8x32 create_application_thunk( u8x32 handle )
{
  auto fix_handle = Handle<Fix>::forge( handle );
  optional<Handle<AnyTree>> h
    = handle::extract<ExpressionTree>( fix_handle )
        .transform( []( auto h ) -> Handle<AnyTree> { return h; } )
        .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ObjectTree>( fix_handle ); } )
        .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ValueTree>( fix_handle ); } );

  check( h );

  return Handle<Thunk>( Handle<Application>( handle::upcast( h.value() ) ) ).into<Fix>().content;
}

u8x32 create_identification_thunk( u8x32 handle )
{
  auto h = handle::extract<Value>( Handle<Fix>::forge( handle ) ).transform( []( auto h ) {
    return h.template into<Identification>().template into<Thunk>();
  } );

  check( h );
  return h->into<Fix>().content;
}

u8x32 create_selection_thunk( u8x32 handle )
{
  auto h = handle::extract<ObjectTree>( Handle<Fix>::forge( handle ) ).transform( []( auto h ) {
    return h.template into<Selection>().template into<Thunk>();
  } );

  check( h );
  return h->into<Fix>().content;
}

uint32_t get_length( u8x32 handle )
{
  auto fix = Handle<Fix>::forge( handle );
  return handle::extract<ExpressionTree>( fix )
    .transform( []( auto h ) -> Handle<AnyTree> { return h; } )
    .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ObjectTree>( fix ); } )
    .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ValueTree>( fix ); } )
    .transform( [&]( auto h ) { return storage->get( h )->size(); } )
    .or_else( [&]() -> optional<size_t> { return handle::size( fix ); } )
    .value();
}

u8x32 create_strict_encode( u8x32 handle )
{
  auto h = handle::extract<Thunk>( Handle<Fix>::forge( handle ) ).transform( []( auto h ) {
    return h.template into<Strict>().template into<Encode>();
  } );

  check( h );
  return h->into<Fix>().content;
}

u8x32 create_shallow_encode( u8x32 handle )
{
  auto h = handle::extract<Thunk>( Handle<Fix>::forge( handle ) ).transform( []( auto h ) {
    return h.template into<Shallow>().template into<Encode>();
  } );

  check( h );
  return h->into<Fix>().content;
}

uint32_t is_equal( u8x32 lhs, u8x32 rhs )
{
  auto lhs_h = handle::extract<Value>( Handle<Fix>::forge( lhs ) );
  auto rhs_h = handle::extract<Value>( Handle<Fix>::forge( rhs ) );

  check( lhs_h );
  check( rhs_h );

  return *lhs_h == *rhs_h;
}

uint32_t is_blob( u8x32 handle )
{
  return handle::extract<Blob>( Handle<Fix>::forge( handle ) ).has_value();
}

uint32_t is_tree( u8x32 handle )
{
  auto fix_handle = Handle<Fix>::forge( handle );
  optional<Handle<AnyTree>> h
    = handle::extract<ExpressionTree>( fix_handle )
        .transform( []( auto h ) -> Handle<AnyTree> { return h; } )
        .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ObjectTree>( fix_handle ); } )
        .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ValueTree>( fix_handle ); } );

  return h.has_value();
}

uint32_t is_tag( u8x32 handle )
{
  auto fix_handle = Handle<Fix>::forge( handle );
  optional<Handle<AnyTree>> h
    = handle::extract<ExpressionTree>( fix_handle )
        .transform( []( auto h ) -> Handle<AnyTree> { return h; } )
        .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ObjectTree>( fix_handle ); } )
        .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ValueTree>( fix_handle ); } );

  if ( h.has_value() ) {
    return h->visit<bool>( []( auto h ) { return h.is_tag(); } );
  } else {
    return false;
  }
}

uint32_t is_blob_ref( u8x32 handle )
{
  return handle::extract<BlobRef>( Handle<Fix>::forge( handle ) ).has_value();
}

uint32_t is_tree_ref( u8x32 handle )
{
  return handle::extract<ValueTreeRef>( Handle<Fix>::forge( handle ) ).has_value()
         || handle::extract<ObjectTreeRef>( Handle<Fix>::forge( handle ) ).has_value();
}

uint32_t is_value( u8x32 handle )
{
  return handle::kind( Handle<Fix>::forge( handle ) ) == FixKind::Value;
}

uint32_t is_object( u8x32 handle )
{
  return handle::kind( Handle<Fix>::forge( handle ) ) == FixKind::Object;
}

uint32_t is_thunk( u8x32 handle )
{
  return handle::extract<Thunk>( Handle<Fix>::forge( handle ) ).has_value();
}

uint32_t is_encode( u8x32 handle )
{
  return handle::extract<Encode>( Handle<Fix>::forge( handle ) ).has_value();
}

uint32_t is_strict( u8x32 handle )
{
  return handle::extract<Strict>( Handle<Fix>::forge( handle ) ).has_value();
}

uint32_t is_shallow( u8x32 handle )
{
  return handle::extract<Shallow>( Handle<Fix>::forge( handle ) ).has_value();
}
}
