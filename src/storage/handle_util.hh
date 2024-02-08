#pragma once
#include <string_view>

#include "blake3.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "object.hh"

namespace handle {
static inline Handle<Blob> create( const BlobData& blob )
{
  if ( blob->size() < Handle<Literal>::MAXIMUM_LENGTH ) {
    return Handle<Literal>( { blob->span().data(), blob->size() } );
  }
  u8x32 hash = blake3::encode( std::as_bytes( blob->span() ) );
  return Handle<Named> { hash, blob->size() };
}

static inline FixKind tree_kind( const TreeData& tree )
{
  FixKind kind = FixKind::Value;
  for ( size_t i = 0; i < tree->size(); i++ ) {
    if ( handle::kind( tree->at( i ) ) > kind ) {
      kind = handle::kind( tree->at( i ) );
    }
  }
  return kind;
}

static inline Handle<AnyTree> create( const TreeData& data )
{
  u8x32 hash = blake3::encode( std::as_bytes( data->span() ) );
  switch ( tree_kind( data ) ) {
    case FixKind::Value:
      return Handle<ValueTree>( hash, data->size() );
    case FixKind::Object:
      return Handle<ObjectTree>( hash, data->size() );
    case FixKind::Expression:
      return Handle<ExpressionTree>( hash, data->size() );
    case FixKind::Fix:
      throw std::runtime_error( "invalid contents of tree" );
  }
  __builtin_unreachable();
}
};
