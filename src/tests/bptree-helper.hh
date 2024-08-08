#include "bptree.hh"
#include "test.hh"
#include <memory>

namespace bptree {
static Handle<Value> to_storage( RuntimeStorage& storage, const std::shared_ptr<Node>& node )
{
  size_t tree_size = 1 + ( node->is_leaf() ? node->get_data().size() : node->get_children().size() );
  auto tree = OwnedMutTree::allocate( tree_size );
  const auto& keys = node->get_keys();

  auto blob = OwnedMutBlob::allocate( keys.size() * sizeof( uint64_t ) );
  memcpy( blob.data(), keys.data(), keys.size() * sizeof( uint64_t ) );
  tree[0] = storage.create( std::make_shared<OwnedBlob>( std::move( blob ) ) );

  size_t i = 1;
  if ( node->is_leaf() ) {
    for ( const auto& data : node->get_data() ) {
      auto d = OwnedMutBlob::allocate( data.size() );
      memcpy( d.data(), data.data(), data.size() );
      tree[i] = storage.create( std::make_shared<OwnedBlob>( std::move( d ) ) );
      i++;
    }
  } else {
    for ( const auto& child : node->get_children() ) {
      tree[i] = to_storage( storage, child );
      i++;
    }
  }

  return storage.ref( storage.create( std::make_shared<OwnedTree>( std::move( tree ) ) ).unwrap<ValueTree>() )
    .unwrap<ValueTreeRef>();
}

static Handle<Value> to_storage( RuntimeStorage& storage, BPTree& tree )
{
  return to_storage( storage, tree.get_root() );
}

static Handle<Value> to_rt( IRuntime& rt, const std::shared_ptr<Node>& node )
{
  size_t tree_size = 1 + ( node->is_leaf() ? node->get_data().size() : node->get_children().size() );
  auto tree = OwnedMutTree::allocate( tree_size );
  const auto& keys = node->get_keys();
  tree[0] = blob(
    rt, std::string_view { reinterpret_cast<const char*>( keys.data() ), keys.size() * sizeof( uint64_t ) } );

  size_t i = 1;
  if ( node->is_leaf() ) {
    for ( const auto& data : node->get_data() ) {
      tree[i] = blob( rt, data );
      i++;
    }
  } else {
    for ( const auto& child : node->get_children() ) {
      tree[i] = to_rt( rt, child );
      i++;
    }
  }

  return Handle<ValueTreeRef>( rt.create( std::make_shared<OwnedTree>( std::move( tree ) ) ).unwrap<ValueTree>(),
                               tree_size );
}

static Handle<Value> to_rt( IRuntime& rt, BPTree& tree )
{
  return to_rt( rt, tree.get_root() );
}
};
