#include "runtimestorage.hh"
#include "handle_util.hh"
#include "overload.hh"
#include "storage_exception.hh"

using namespace std;

Handle<Blob> RuntimeStorage::create( BlobData blob, std::optional<Handle<Blob>> name )
{
  auto handle = name.or_else( [&] -> decltype( name ) { return handle::create( blob ); } ).value();
  handle.visit<void>( overload {
    [&]( Handle<Literal> ) {},
    [&]( Handle<Named> name ) { blobs_.insert( name, blob ); },
  } );
  return handle;
}

Handle<Tree> RuntimeStorage::create( TreeData tree, std::optional<Handle<Tree>> name )
{
  auto handle = name.or_else( [&] -> decltype( name ) { return handle::create( tree ); } ).value();
  trees_.insert( handle, tree );
  return handle;
}

BlobData RuntimeStorage::get( Handle<Named> handle )
{
  auto res = blobs_.get( handle );
  if ( res.has_value() ) {
    return res.value();
  } else {
    throw HandleNotFound( handle );
  }
}

TreeData RuntimeStorage::get( Handle<Tree> handle )
{
  auto res = trees_.get( handle );
  if ( res.has_value() ) {
    return res.value();
  } else {
    throw HandleNotFound( handle );
  }
}

bool RuntimeStorage::contains( Handle<Named> handle )
{
  return blobs_.contains( handle );
}

bool RuntimeStorage::contains( Handle<Tree> handle )
{
  return trees_.contains( handle );
}

optional<Handle<Tree>> RuntimeStorage::contains( Handle<TreeRef> handle )
{
  auto tmp_tree = Handle<Tree>::forge( handle.content );
  auto entry = trees_.get_handle( tmp_tree );

  if ( !entry.has_value() ) {
    return {};
  }

  return entry;
}

Handle<TreeRef> RuntimeStorage::ref( Handle<Tree> handle )
{
  return handle.into<TreeRef>( get( handle )->size() );
}
