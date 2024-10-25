#pragma once

#include "blake3.hh"
#include "handle_post.hh"
#include "object.hh"

namespace handle {
static inline Handle<Blob> create( const BlobData& blob )
{
  if ( blob->size() <= Handle<Literal>::MAXIMUM_LENGTH ) {
    return Handle<Literal>( { blob->span().data(), blob->size() } );
  }
  u8x32 hash = blake3::encode( std::as_bytes( blob->span() ) );
  return Handle<Named> { hash, blob->size() };
}

static inline size_t tree_size( const TreeData& tree )
{
  size_t size = 0;
  for ( size_t i = 0; i < tree->size(); i++ ) {
    size += byte_size( tree->at( i ) );
  }
  size += tree->size() * sizeof( Handle<Fix> );
  return size;
}

static inline Handle<Tree> create( const TreeData& data )
{
  u8x32 hash = blake3::encode( std::as_bytes( data->span() ) );
  return Handle<Tree>( hash, tree_size( data ) );
}
}
