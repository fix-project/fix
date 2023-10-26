#pragma once

#include <absl/container/flat_hash_set.h>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "concurrent_storage.hh"
#include "concurrent_vector.hh"
#include "fixcache.hh"
#include "handle.hh"
#include "object.hh"
#include "program.hh"
#include "spans.hh"
#include "worker.hh"

#include "absl/container/flat_hash_map.h"

#ifndef TRUSTED
#define TRUSTED
#endif

using ObjectOrName = std::variant<OwnedObject, Handle>;

class RuntimeStorage
{
private:
  friend class RuntimeWorker;

  std::shared_mutex storage_mutex_ {};
  // Storage for Object/Handles with a canonical name
  absl::flat_hash_map<Handle, OwnedObject, AbslHash> canonical_storage_ {};
  // Storage for Object/Handles with a local name
  std::vector<ObjectOrName> local_storage_ {};

  // Keeping track of canonical and local task handle translation
  absl::flat_hash_map<Handle, std::list<Handle>, AbslHash> canonical_to_local_cache_for_tasks_ {};

  // Maps a Wasm function Handle to corresponding compiled Program
  std::unordered_multimap<Handle, std::string> friendly_names_ {};

  template<owned_object T>
  T& get_object_owned( Handle name );

  template<mutable_object T>
  T get_object_mut( Handle name );

  template<object T>
  T get_object( Handle name );

  void schedule( Task task );

  std::filesystem::path get_fix_repo();
  void serialize_objects( Handle name, const std::filesystem::path& dir );
  void deserialize_objects( const std::filesystem::path& dir );

public:
  // Take ownership of a Blob
  Handle add_blob( OwnedBlob&& blob );

  // Take ownership of a Tree
  Handle add_tree( OwnedTree&& tree );

  // Return reference to blob content
  Blob get_blob( Handle name );

  // Return reference to tree content
  Tree get_tree( Handle name );

  Handle canonicalize( Handle handle );

  Task canonicalize( Task task );

  void serialize( Handle handle );
  void deserialize();

  // Tests if the Handle (with the specified accessibility) is valid with the current contents.
  bool contains( Handle handle );

  // Gets all the known friendly names for this handle.
  std::vector<std::string> get_friendly_names( Handle handle );

  // Gets the base64 encoded name of the handle.
  std::string get_encoded_name( Handle handle );

  // Gets the shortened base64 encoded name of the handle.
  std::string get_short_name( Handle handle );

  // Gets the best name for this Handle to display to users.
  std::string get_display_name( Handle handle );

  // Looks up a Handle by its ref (e.g., a friendly name)
  std::optional<Handle> get_ref( std::string_view ref );

  // Adds a ref for a Handle in-memory (but does not serialize the ref to disk)
  void set_ref( std::string_view ref, Handle handle );

  /**
   * Call @p visitor for every Handle in the "minimum repo" of @p root, i.e., the set of Handles needed for @p root
   * to be valid as input to a Fix program.
   *
   * The iteration order is such that every child will be visited before its parents.
   *
   * @param root            The Handle from which to start traversing inputs.
   * @param visitor         A function to call on every dependency.
   */
  void visit( Handle root, std::function<void( Handle )> visitor );

  /**
   * Determines if two Handles should be treated as equal.  This might canonicalize the Handles if necessary.
   *
   * @return  true if the Handles refer to the same object, false otherwise
   */
  bool compare_handles( Handle x, Handle y );

  /**
   * Checks if @p root, as well as all its dependencies, are resident in storage.
   */
  bool complete( Handle root );

  /**
   * Computes the minimum repository of @p root.
   */
  std::vector<Handle> minrepo( Handle root );

  /**
   * Return the canonical name if the Name @name has been canonicalized. No work is done if it is not canonicalized
   *
   * @param name            The name for which to request the canonical name
   */
  std::optional<Handle> get_canonical_name( Handle name );

  /**
   * Return the local task if the Task @task has been canonicalized.
   */
  std::list<Task> get_local_tasks( Task task );
};
