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
#include "entry.hh"
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

  FixCache fix_cache_ {};
  absl::flat_hash_map<Handle, Handle, AbslHash> canonical_to_local_ {};
  std::unordered_multimap<Handle, std::string> friendly_names_ {};

  // Maps a Wasm function Handle to corresponding compiled Program
  InMemoryStorage<Program> name_to_program_ {};

  // Storage for Object/Handles with a local name
  concurrent_vector<Object> local_storage_ {};

  std::vector<std::unique_ptr<RuntimeWorker>> workers_ {};

  size_t num_workers_;

  std::atomic<bool> threads_active_ = true;
  std::atomic<bool> threads_started_ = false;

  std::atomic<size_t> work_ = 0;

  inline static thread_local Handle current_procedure_;
  inline static thread_local bool nondeterministic_api_allowed_ = false;

  std::optional<Handle> scheduler_procedure_;

  RuntimeStorage()
    : num_workers_( 16 )
    , scheduler_procedure_( std::nullopt )
  {
    wasm_rt_init();

    for ( size_t i = 0; i < num_workers_; ++i ) {
      std::unique_ptr<RuntimeWorker> worker = std::make_unique<RuntimeWorker>( i, *this );
      workers_.push_back( std::move( worker ) );
    }

    threads_started_ = true;
    threads_started_.notify_all();
  }

  ~RuntimeStorage()
  {
    threads_active_ = false;
    work_ = 1;
    work_.notify_all();

    for ( size_t i = 0; i < num_workers_; ++i ) {
      ( *workers_.at( i ) ).thread_.join();
    }
  }

  void schedule( Task task );

  RuntimeWorker& current_worker() { return *workers_.at( RuntimeWorker::current_thread_id_ ); }

  std::filesystem::path get_fix_repo();

public:
  // Return reference to static runtime storage
  static RuntimeStorage& get_instance()
  {
    static RuntimeStorage runtime_instance;
    return runtime_instance;
  }

  void set_scheduler( Handle& scheduler )
  {
    if ( scheduler_procedure_ ) {
      throw std::runtime_error( "Attempted to overwrite scheduler procedure." );
    }
    scheduler_procedure_ = scheduler;
  }

  bool steal_work( Task& task, size_t tid );

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

  // Blocking eval operations
  Handle eval_thunk( Handle name );

  // Return encode name referred to by thunk
  Handle get_thunk_encode_name( Handle thunk_name );

  // Populate a program
  void populate_program( Handle function_name );

  void add_program( Handle function_name, std::string_view elf_content );

  Handle local_to_storage( Handle name );

  std::string serialize( Handle name );
  std::string serialize_to_dir( Handle name, const std::filesystem::path& dir );
  void deserialize();
  void deserialize_from_dir( const std::filesystem::path& dir );

  void set_current_procedure( const Handle function_name ) { current_procedure_ = function_name; }

  Handle get_current_procedure() const { return current_procedure_; }

  // Get dependees. A list of tasks that must complete before this task may be unblocked
  std::vector<Task> get_dependees( Task depender ) { return fix_cache_.get_dependees( depender ); }
  // Get status. Returns none if the entry does not exist or does exist and is std::nullopt.
  // Returns value(handle) if the handle exists in cache.
  std::optional<Handle> get_status( Task task ) { return fix_cache_.get( task ); };

  // get entries in local storage for debugging purposes
  Object& local_storage_at( size_t index ) { return local_storage_.at( index ); }

  size_t get_local_storage_size() { return local_storage_.size(); }

  Handle get_local_handle( Handle canonical ) { return canonical_to_local_.at( canonical ); }

  // Get the tasks that produced a handle. Linear time in size of fixcache
  std::vector<Task> get_parents( Handle child ) { return fix_cache_.get_parents( child ); }

  // Tests if the Handle (with the specified accessibility) is valid with the current contents.
  bool contains( Handle handle );

  // Gets the evaluated version of the Handle, if it's been computed already.
  std::optional<Handle> get_evaluated( Handle handle );

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

  // Checks if RuntimeStorage has marked the currently-executing thread as nondeterministic.
  static bool nondeterministic_api_allowed()
  {
    return RuntimeStorage::get_instance().nondeterministic_api_allowed_;
  }
};
