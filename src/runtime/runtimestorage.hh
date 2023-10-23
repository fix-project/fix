#pragma once

#include <condition_variable>
#include <filesystem>
#include <map>
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

class RuntimeStorage
{
private:
  friend class RuntimeWorker;

  InMemoryStorage<Handle> canonical_to_local_ {};
  std::unordered_multimap<Handle, std::string> friendly_names_ {};

  // Maps a Wasm function Handle to corresponding compiled Program
  InMemoryStorage<Program> name_to_program_ {};

  // Storage for Object/Handles with a local name
  concurrent_vector<Object> local_storage_ {};

  void schedule( Task task );

  std::filesystem::path get_fix_repo();
  std::string serialize_objects( Handle name, const std::filesystem::path& dir );
  void deserialize_objects( const std::filesystem::path& dir );

public:
  // add blob
  Handle add_blob( Blob&& blob );

  // Return reference to blob content
  std::string_view get_blob( Handle name );
  std::string_view user_get_blob( const Handle& name );

  // add Tree
  Handle add_tree( Tree&& tree );

  // add Tag
  Handle add_tag( Tree&& tree );

  // Return reference to Tree
  span_view<Handle> get_tree( Handle name );

  // add Thunk
  Handle add_thunk( Thunk thunk );

  // Return encode name referred to by thunk
  Handle get_thunk_encode_name( Handle thunk_name );

  // Populate a program
  void populate_program( Handle function_name );

  void add_program( Handle function_name, std::string_view elf_content );

  Handle canonicalize( Handle name );

  std::optional<Handle> get_local_name( Handle name );

  std::string serialize( Handle handle );
  void deserialize();

  // get entries in local storage for debugging purposes
  Object& local_storage_at( size_t index ) { return local_storage_.at( index ); }

  size_t get_local_storage_size() { return local_storage_.size(); }

  Handle get_local_handle( Handle canonical ) { return canonical_to_local_.get( canonical ); }

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
};
