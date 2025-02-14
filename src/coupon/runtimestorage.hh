#pragma once

#include <string_view>
#include <unordered_set>

#include "handle.hh"
#include "hash_table.hh"
#include "object.hh"

#ifndef TRUSTED
#define TRUSTED
#endif

struct AbslHash
{
  template<typename T>
  std::size_t operator()( Handle<T> const& handle ) const noexcept
  {
    u64x4 dwords = (u64x4)handle.content;
    return ( dwords[0] );
  }
};

class RuntimeStorage
{
private:
  friend class RuntimeWorker;
  using BlobMap = FixTable<Named, BlobData, AbslHash>;
  using TreeMap = FixTable<Tree, TreeData, AbslHash>;

  BlobMap blobs_ { 100000 };
  TreeMap trees_ { 100000 };
  // TreeMap tree_refs_ { 100000 };

public:
  RuntimeStorage() {}

  // Construct a Blob by taking ownership of a memory region
  Handle<Blob> create( BlobData blob, std::optional<Handle<Blob>> name = {} );

  // Construct a Tree by taking ownership of a memory region
  Handle<Tree> create( TreeData tree, std::optional<Handle<Tree>> name = {} );

  // Construct a TreeRef with TreeData
  // Handle<Tree> create_tree_shallow( TreeData tree, std::optional<Handle<Tree>> name = {} );

  // Construct a Blob by copying a memory region.
  Handle<Blob> create( const std::string_view string )
  {
    auto blob = OwnedMutBlob::allocate( string.size() );
    memcpy( blob.data(), string.data(), string.size() );
    return create( std::make_shared<OwnedBlob>( std::move( blob ) ) );
  }

  // Construct a Tree by copying a memory region.
  Handle<Tree> create( const std::span<const Handle<Fix>> span )
  {
    auto tree = OwnedMutTree::allocate( span.size() );
    memcpy( tree.data(), span.data(), span.size_bytes() );
    return create( std::make_shared<OwnedTree>( std::move( tree ) ) );
  }

  // Return reference to blob content.
  BlobData get( Handle<Named> name );
  // Return reference to tree content.
  TreeData get( Handle<Tree> name );
  // Return reference to tree content.
  // std::shared_ptr<OwnedTree> get_shallow( Handle<Tree> name );

  // Tests if the data corresponding to the current handle is stored.
  bool contains( Handle<Named> handle );
  bool contains( Handle<Tree> handle );
  // bool contains_shallow( Handle<Tree> handle );

  // return the reffed Handle<AnyTree> if known
  std::optional<Handle<Tree>> contains( Handle<TreeRef> handle );
  Handle<TreeRef> ref( Handle<Tree> tree );

  /**
   * Call @p visitor for every Handle in the "fully accessible repo" of @p root, i.e., the set of Handles which
   * might ever be needed in the recursive evaluation of a procedure which receives @p root in its input.
   *
   * The iteration order is such that every child will be visited before its parents.
   *
   * @param root            The Handle from which to start traversing inputs.
   * @param visitor         A function to call on every dependency.
   */
  template<FixType T>
  void visit_full(
    Handle<T> root,
    std::function<void( Handle<Fix> )> visitor,
    std::optional<std::function<void( Handle<Fix>, const std::unordered_set<Handle<Fix>>& )>> pin_visitor
    = std::nullopt,
    std::unordered_set<Handle<Fix>> visited = {} );
};
