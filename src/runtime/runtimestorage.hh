#pragma once

#include <absl/container/flat_hash_set.h>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <stdio.h>
#include <string>
#include <string_view>
#include <unordered_map>

#include "handle.hh"
#include "object.hh"
#include "program.hh"
#include "task.hh"

#include "absl/container/flat_hash_map.h"

#ifndef TRUSTED
#define TRUSTED
#endif

using DatumOrName = std::variant<OwnedSpan, Handle<Fix>>;

struct AbslHash
{
  template<typename T>
  std::size_t operator()( Handle<T> const& handle ) const noexcept
  {
    u64x4 dwords = (u64x4)handle.content;
    return absl::Hash<uint64_t> {}( dwords[0] );
  }
};

class RuntimeStorage
{
private:
  friend class RuntimeWorker;

  std::shared_mutex storage_mutex_ {};
  // Storage for Object/Handles with a canonical name
  absl::flat_hash_map<Handle<Fix>, DatumOrName, AbslHash> canonical_storage_ {};
  // Storage for Object/Handles with a local name
  std::vector<DatumOrName> local_storage_ {};
  absl::flat_hash_map<Handle<Relation>, Handle<Fix>, AbslHash> local_relations_ {};

  // Keeping track of canonical and local task handle translation
  absl::flat_hash_map<Handle<Fix>, std::list<Handle<Fix>>, AbslHash> canonical_to_local_cache_for_tasks_ {};

  /* // Keepying track of what objects are pinned by an object */
  absl::flat_hash_map<Handle<Fix>, std::unordered_set<Handle<Fix>>, AbslHash> pins_ {};

  // Maps a Wasm function Handle to corresponding compiled Program
  std::unordered_map<Handle<Blob>, Program> linked_programs_ {};

  // Maps a Handle to its user-facing names
  std::unordered_multimap<Handle<Fix>, std::string> labels_ {};

  void schedule( Task task );

  template<FixType T>
  Handle<T> canonicalize( Handle<T> handle, std::unique_lock<std::shared_mutex>& lock );

  BlobSpan get_unsync( const Handle<Blob>& name );
  template<FixTreeType T>
  TreeSpan get_unsync( Handle<T> name );
  Handle<Fix> get_unsync( Handle<Relation> handle );

public:
  RuntimeStorage() { canonical_storage_.reserve( 1024 ); }

  std::filesystem::path get_fix_repo();

  // Construct a Blob by taking ownership of a memory region
  Handle<Blob> create( OwnedBlob&& blob, std::optional<Handle<Fix>> name = {} );

  // Construct a Tree by taking ownership of a memory region
  Handle<Fix> create( OwnedTree&& tree, std::optional<Handle<Fix>> name = {} );

  // Construct a Relation
  void create( Handle<Relation> relation, Handle<Fix> result );

  template<FixTreeType T>
  Handle<T> create_tree( OwnedTree&& tree, std::optional<Handle<Fix>> name = {} );

  // Construct a Blob by copying a memory region.
  Handle<Blob> create( const std::string_view string )
  {
    auto blob = OwnedMutBlob::allocate( string.size() );
    memcpy( blob.data(), string.data(), string.size() );
    return create( std::move( blob ) );
  }

  // Construct a Tree by copying a memory region.
  Handle<Fix> create( const std::span<const Handle<Fix>> span )
  {
    auto tree = OwnedMutTree::allocate( span.size() );
    memcpy( tree.data(), span.data(), span.size_bytes() );
    return create( std::move( tree ) );
  }

  template<FixTreeType T>
  Handle<T> create_tree( const std::span<const Handle<typename Handle<T>::element_type>> span )
  {
    auto tree = OwnedMutTree::allocate( span.size() );
    for ( size_t i = 0; i < span.size(); i++ ) {
      tree[i] = span[i];
    }
    return create_tree<T>( std::move( tree ) );
  }

  // Construct a Tree by copying individual elements.
  template<FixHandle... As>
  Handle<Fix> construct( As... args )
  {
    return ( [&]( std::initializer_list<Handle<Fix>> list ) {
      return create( { std::data( list ), list.size() } );
    } )( { args... } );
  }

  template<FixTreeType T, FixHandle... As>
  Handle<T> construct_tree( As... args )
  {
    return ( [&]( std::initializer_list<Handle<typename Handle<T>::element_type>> list ) {
      return create_tree<T>( { std::data( list ), list.size() } );
    } )( { args... } );
  }

  // Return reference to blob content.  The returned span is only valid for the lifetime of the Handle.
  BlobSpan get( const Handle<Blob>& name );

  // Return reference to tree content.
  template<FixTreeType T>
  TreeSpan get( Handle<T> name );

  // Get the result of a relation.
  Handle<Fix> get( Handle<Relation> name );

  // Convert a Handle into the canonically-named version of that handle.
  template<FixType T>
  Handle<T> canonicalize( Handle<T> handle )
  {
    std::unique_lock lock( storage_mutex_ );
    return canonicalize( handle, lock );
  }

  Task canonicalize( Task task );

  // Sets a human-readable label for a Handle.
  void label( Handle<Fix> target, const std::string_view label );
  // Looks up a Handle based on the human-readable labels.
  std::optional<Handle<Fix>> label( const std::string_view label );
  // Looks up all the human-readable labels.
  std::unordered_set<std::string> labels( std::optional<Handle<Fix>> handle = {} );

  void deserialize();

  Handle<Fix> serialize( Handle<Fix> root, bool pins = true );
  Handle<Fix> serialize( const std::string_view label, bool pins = true );

  // Tests if the data corresponding to the current handle is stored.
  bool contains( Handle<Fix> handle );

  // Gets all the known friendly names for this handle.
  std::vector<std::string> get_friendly_names( Handle<Fix> handle );

  // Gets the base16 encoded name of the handle.
  std::string get_encoded_name( Handle<Fix> handle );

  // Gets the shortened base16 encoded name of the handle.
  std::string get_short_name( Handle<Fix> handle );

  // Gets the handle given shortened base16 encoded name.
  std::optional<Handle<Fix>> get_handle( const std::string_view short_name );

  // Gets the best name for this Handle to display to users.
  std::string get_display_name( Handle<Fix> handle );

  // Tries to turn the provided string into a Handle using any known method.
  std::optional<Handle<Fix>> lookup( const std::string_view ref );

  /**
   * Call @p visitor for every Handle in the "minimum repo" of @p root, i.e., the set of Handles which are needed
   * to execute a procedure which receives @p root in its input.
   *
   * The iteration order is such that every child will be visited before its parents.
   *
   * @param root            The Handle from which to start traversing inputs.
   * @param visitor         A function to call on every dependency.
   */
  template<FixType T>
  void visit( Handle<T> root,
              std::function<void( Handle<Fix> )> visitor,
              std::unordered_set<Handle<Fix>> visited = {} );

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

  /**
   * Determines if two Handles should be treated as equal.  This might canonicalize the Handles if necessary.
   *
   * @return  true if the Handles refer to the same object, false otherwise
   */
  bool compare_handles( Handle<Fix> x, Handle<Fix> y );

  /**
   * Checks if @p root, as well as all its dependencies, are resident in storage.
   */
  bool complete( Handle<Fix> root );

  /**
   * Computes the minimum repository of @p root.
   */
  std::vector<Handle<Fix>> minrepo( Handle<Fix> root );

  /**
   * Return the canonical name if the Name @name has been canonicalized. No work is done if it is not canonicalized
   *
   * @param name            The name for which to request the canonical name
   */
  std::optional<Handle<Fix>> get_canonical_name( Handle<Fix> name );

  /**
   * Links an ELF file and returns a runnable Program.
   */
  const Program& link( Handle<Blob> handle );

  /**
   * Add an advisory pin to @p dst from @p src.
   */
  void pin( Handle<Fix> src, Handle<Fix> dst );

  /**
   * Return handles pinned by @p handle.
   */
  std::unordered_set<Handle<Fix>> pins( Handle<Fix> handle );

  /**
   * Return handles of tags that tag @p handle.
   */
  std::unordered_set<Handle<Fix>> tags( Handle<Fix> handle );
};
