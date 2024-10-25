#include "runtime.hh"
#include "handle.hh"
#include "object.hh"
#include "overload.hh"
#include <memory>

using namespace std;

BlobData TestRuntime::attach( Handle<Blob> blob )
{
  return blob.visit<BlobData>( overload {
    [&]( Handle<Named> n ) { return storage_.get( n ); },
    []( Handle<Literal> l ) {
      auto data = OwnedMutBlob::allocate( l.size() );
      memcpy( data.data(), l.data(), l.size() );
      return make_shared<OwnedBlob>( move( data ) );
    },
  } );
}

TreeData TestRuntime::attach( Handle<Treeish> tree )
{
  Handle<Tree> t = tree.visit<Handle<Tree>>( overload {
    []( Handle<Tree> x ) { return x; }, []( Handle<Tag> t ) { return Handle<Tree>( t.content, t.size() ); } } );
  return storage_.get( t );
}

size_t TestRuntime::length( Handle<Value> value )
{
  return value.visit<size_t>( overload { [&]( Handle<Treeish> x ) { return this->attach( x )->size(); },
                                         []( auto x ) { return size( x ); } } );
}

size_t TestRuntime::recursive_size( Handle<Treeish> tree )
{
  return tree.visit<size_t>( []( auto x ) { return x.template size(); } );
}

Handle<Blob> TestRuntime::create_blob( BlobData blob )
{
  return storage_.create( blob );
}

Handle<Tree> TestRuntime::create_tree( TreeData tree )
{
  return storage_.create( tree );
}

Handle<Tag> TestRuntime::create_tag( OwnedMutTree&& tree )
{
  tree.at( 0 ) = current_procedure_;
  auto thandle = storage_.create( make_shared<OwnedTree>( std::move( tree ) ) );
  return Handle<Tag>( thandle.hash(), thandle.size() );
}

Handle<Thunk> TestRuntime::create_application_thunk( Handle<Tree> tree )
{
  return Handle<Application>( tree );
}

Handle<Thunk> TestRuntime::create_selection_thunk( Handle<Tree> tree )
{
  return Handle<Selection>( tree );
}

Handle<Thunk> TestRuntime::create_identification_thunk( Handle<Value> value )
{
  return Handle<Identification>( value );
}

Handle<Encode> TestRuntime::create_strict_encode( Handle<Thunk> thunk )
{
  return Handle<Strict>( thunk );
}

Handle<Encode> TestRuntime::create_shallow_encode( Handle<Thunk> thunk )
{
  return Handle<Shallow>( thunk );
}

bool TestRuntime::is_blob( Handle<Fix> handle )
{
  return handle.try_into<Value>().and_then( []( auto x ) { return x.template try_into<Blob>(); } ).has_value();
}

bool TestRuntime::is_tree( Handle<Fix> handle )
{
  return handle.try_into<Value>()
    .and_then( []( auto x ) { return x.template try_into<Treeish>(); } )
    .and_then( []( auto x ) { return x.template try_into<Tree>(); } )
    .has_value();
}

bool TestRuntime::is_tag( Handle<Fix> handle )
{
  return handle.try_into<Value>()
    .and_then( []( auto x ) { return x.template try_into<Treeish>(); } )
    .and_then( []( auto x ) { return x.template try_into<Tag>(); } )
    .has_value();
}

bool TestRuntime::is_thunk( Handle<Fix> handle )
{
  return handle.try_into<Thunk>().has_value();
}

bool TestRuntime::is_encode( Handle<Fix> handle )
{
  return handle.try_into<Encode>().has_value();
}

Handle<TreeishRef> TestRuntime::ref( Handle<Treeish> tree )
{
  auto length = attach( tree )->size();
  return tree.visit<Handle<TreeishRef>>( overload {
    [&]( Handle<Tree> t ) { return t.into<TreeRef>( length ); },
    [&]( Handle<Tag> t ) { return t.into<TagRef>( length ); },
  } );
}

Handle<BlobRef> TestRuntime::ref( Handle<Blob> blob )
{
  return Handle<BlobRef>( blob );
}

Handle<Fix> TestRuntime::ref( Handle<Fix> handle )
{
  if ( is_blob( handle ) ) {
    return Handle<BlobRef>( handle.unwrap<Value>().unwrap<Blob>() );
  }
  if ( is_tag( handle ) or is_tree( handle ) ) {
    return ref( handle.unwrap<Value>().unwrap<Treeish>() );
  }

  return handle;
}

Handle<Tag> TestRuntime::create_arbitrary_tag( TreeData tree )
{
  auto thandle = storage_.create( tree );
  return Handle<Tag>( thandle.hash(), thandle.size() );
}

Handle<Blob> TestRuntime::load( Handle<Blob> x )
{
  return x;
}

Handle<Blob> TestRuntime::load( Handle<BlobRef> x )
{
  return x.unwrap<Blob>();
}

Handle<Treeish> TestRuntime::load( Handle<Treeish> x )
{
  return x;
}

Handle<Treeish> TestRuntime::load( Handle<TreeishRef> x )
{
  Handle<TreeRef> tmp = x.visit<Handle<TreeRef>>( overload {
    []( Handle<TreeRef> x ) { return x; },
    []( Handle<TagRef> x ) { return Handle<TreeRef>::forge( x.content ); },
  } );
  auto tree = storage_.contains( tmp ).value();
  return x.visit<Handle<Treeish>>( overload {
    [&]( Handle<TreeRef> ) { return tree; },
    [&]( Handle<TagRef> ) { return Handle<Tag>( tree.hash(), tree.size() ); },
  } );
}

Handle<Blob> TestRuntime::unref( Handle<BlobRef> x )
{
  return x.unwrap<Blob>();
}

Handle<Treeish> TestRuntime::unref( Handle<TreeishRef> x )
{
  return load( x );
}

u8x32 TestRuntime::get_name( Handle<Fix> handle )
{
  return handle.content;
}

u8x32 TestRuntime::get_canonical_name( Handle<Fix> handle )
{
  return handle.content;
}

Handle<Thunk> TestRuntime::unwrap( Handle<Encode> encode )
{
  return encode.visit<Handle<Thunk>>( []( auto x ) { return x.template unwrap<Thunk>(); } );
}

Handle<Tree> TestRuntime::unwrap( Handle<Thunk> thunk )
{
  return thunk.unwrap<Application>().unwrap<Tree>();
}

KernelExecutionTag TestRuntime::execute( Handle<Blob> machine_code, Handle<Tree> combination )
{
  current_procedure_ = machine_code;
  auto result = runner_.execute( machine_code, combination );
  return KernelExecutionTag { .procedure = machine_code, .combination = combination, .result = result };
}
