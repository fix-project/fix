#pragma once
#include <string_view>

#include "handle.hh"
#include "object.hh"
#include "sha256.hh"

namespace handle {
static inline Handle<Blob> create( const BlobData& blob )
{
  if ( blob->size() < Handle<Literal>::MAXIMUM_LENGTH ) {
    return Handle<Literal>( { blob->span().data(), blob->size() } );
  }
  std::string str = sha256::encode_span( blob->span() );
  u8x32 hash;
  memcpy( &hash, str.data(), 32 );
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
  std::string str = sha256::encode_span( data->span() );
  u8x32 hash;
  memcpy( &hash, str.data(), 32 );
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
