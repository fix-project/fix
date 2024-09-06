#include "bptree.hh"
#include "handle.hh"
#include "object.hh"
#include "test.hh"
#include <memory>

namespace bptree {
static Handle<Blob> to_storage_keys( RuntimeStorage& storage, Node* node )
{
  const auto& keys = node->get_keys();
  auto blob = OwnedMutBlob::allocate( keys.size() * sizeof( int ) + 1 );
  blob[0] = node->is_leaf();
  memcpy( blob.data() + 1, keys.data(), keys.size() * sizeof( int ) );
  return storage.create( std::make_shared<OwnedBlob>( std::move( blob ) ) );
}

static std::deque<Handle<ValueTreeRef>> to_storage_leaves( RuntimeStorage& storage, BPTree& bptree )
{
  static auto nil
    = storage.create( std::make_shared<OwnedTree>( OwnedMutTree::allocate( 0 ) ) ).unwrap<ValueTree>();

  std::deque<Node*> leaf_nodes;
  bptree.dfs_visit( [&]( Node* node ) {
    if ( node->is_leaf() ) {
      leaf_nodes.push_back( node );
    }
  } );

  std::deque<Handle<ValueTreeRef>> result;
  std::optional<Handle<ValueTreeRef>> last;
  while ( !leaf_nodes.empty() ) {
    auto node = leaf_nodes.back();
    leaf_nodes.pop_back();

    auto tree = OwnedMutTree::allocate( 1 + bptree.get_degree() + 1 );
    tree[0] = to_storage_keys( storage, node )
                .visit<Handle<Value>>( overload {
                  []( Handle<Literal> l ) { return l; },
                  []( Handle<Named> n ) { return Handle<BlobRef>( n ); },
                } );

    size_t i = 1;
    for ( const auto& data : node->get_data() ) {
      auto d = OwnedMutBlob::allocate( data.size() );
      memcpy( d.data(), data.data(), data.size() );
      tree[i] = storage.create( std::make_shared<OwnedBlob>( std::move( d ) ) )
                  .visit<Handle<Value>>( overload { []( Handle<Literal> l ) { return l; },
                                                    []( Handle<Named> n ) { return Handle<BlobRef>( n ); } } );
      i++;
    }

    for ( ; i <= bptree.get_degree(); i++ ) {
      tree[i] = Handle<ValueTreeRef>( nil, 0 );
    }

    if ( last.has_value() ) {
      tree[i] = last.value();
    } else {
      tree[i] = Handle<ValueTreeRef>( nil, 0 );
    }

    last = storage.ref( storage.create( std::make_shared<OwnedTree>( std::move( tree ) ) ) ).unwrap<ValueTreeRef>();
    result.push_back( last.value() );
  }

  return result;
}

static std::deque<Handle<ValueTreeRef>> to_repo_leaves( RuntimeStorage& storage, Repository& repo, BPTree& bptree )
{
  auto res = to_storage_leaves( storage, bptree );

  for ( auto l : res ) {
    auto unref = storage.contains( l ).value();
    auto tree = storage.get( unref );
    repo.put( unref, tree );
    for ( auto d : tree->span() ) {
      d.unwrap<Expression>().unwrap<Object>().unwrap<Value>().visit<void>(
        overload { [&]( Handle<BlobRef> b ) {
                    auto unref = b.unwrap<Blob>().unwrap<Named>();
                    auto blob = storage.get( unref );
                    repo.put( unref, blob );
                  },
                   []( auto ) {} } );
    }
  }

  return res;
}

static Handle<Value> to_storage_internal( RuntimeStorage& storage,
                                          const std::shared_ptr<Node>& node,
                                          std::deque<Handle<ValueTreeRef>>& leaves )
{
  if ( node->is_leaf() ) {
    auto result = leaves.back();
    leaves.pop_back();
    return result;
  } else {
    size_t tree_size = 1 + ( node->is_leaf() ? node->get_data().size() : node->get_children().size() );
    auto tree = OwnedMutTree::allocate( tree_size );
    tree[0] = to_storage_keys( storage, node.get() )
                .visit<Handle<Value>>( overload {
                  []( Handle<Literal> l ) { return l; },
                  []( Handle<Named> n ) { return Handle<BlobRef>( n ); },
                } );

    size_t i = 1;
    for ( const auto& child : node->get_children() ) {
      tree[i] = to_storage_internal( storage, child, leaves );
      i++;
    }

    return storage.ref( storage.create( std::make_shared<OwnedTree>( std::move( tree ) ) ).unwrap<ValueTree>() )
      .unwrap<ValueTreeRef>();
  }
}

static Handle<Value> to_repo_internal( RuntimeStorage& storage,
                                       Repository& repo,
                                       const std::shared_ptr<Node>& node,
                                       std::deque<Handle<ValueTreeRef>>& leaves )
{
  if ( node->is_leaf() ) {
    auto result = leaves.back();
    leaves.pop_back();
    return result;
  } else {
    size_t tree_size = 1 + ( node->is_leaf() ? node->get_data().size() : node->get_children().size() );
    auto tree = OwnedMutTree::allocate( tree_size );
    tree[0] = to_storage_keys( storage, node.get() )
                .visit<Handle<Value>>( overload {
                  []( Handle<Literal> l ) { return l; },
                  [&]( Handle<Named> n ) {
                    repo.put( n, storage.get( n ) );
                    return Handle<BlobRef>( n );
                  },
                } );

    size_t i = 1;
    for ( const auto& child : node->get_children() ) {
      tree[i] = to_repo_internal( storage, repo, child, leaves );
      i++;
    }

    auto res = storage.ref( storage.create( std::make_shared<OwnedTree>( std::move( tree ) ) ).unwrap<ValueTree>() )
                 .unwrap<ValueTreeRef>();
    auto unref = storage.contains( res ).value();
    repo.put( unref, storage.get( unref ) );

    return res;
  }
}

static Handle<Value> to_storage( RuntimeStorage& storage, BPTree& tree )
{
  auto leaves = to_storage_leaves( storage, tree );
  return to_storage_internal( storage, tree.get_root(), leaves );
}

static Handle<Value> to_repo( RuntimeStorage& storage, Repository& repo, BPTree& tree )
{
  auto leaves = to_repo_leaves( storage, repo, tree );
  return to_repo_internal( storage, repo, tree.get_root(), leaves );
}
};
