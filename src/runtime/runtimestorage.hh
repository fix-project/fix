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

  std::thread scheduler_thread_ {};
  boost::lockfree::queue<Task> schedulable_tasks_;
  std::atomic<size_t> schedulable_ = 0;

  RuntimeStorage()
    : num_workers_( 16 )
    , schedulable_tasks_( 0 )
  {
    wasm_rt_init();
    scheduler_thread_ = std::thread( std::bind( &RuntimeStorage::schedule, this ) );

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
    schedulable_ = 1;
    schedulable_.notify_all();

    for ( size_t i = 0; i < num_workers_; ++i ) {
      ( *workers_.at( i ) ).thread_.join();
    }
    scheduler_thread_.join();
  }

  void schedule_task( Task task )
  {
    schedulable_tasks_.push( task );
    schedulable_++;
    schedulable_.notify_all();
  }

  void schedule()
  {
    schedulable_.wait( 0 );
    while ( threads_active_ ) {
      Task task;
      bool popped = schedulable_tasks_.pop( task );
      schedulable_--;
      if ( not popped ) {
        throw std::runtime_error( "couldn't pop task" );
      }
      workers_.at( 0 )->runq_.push( task );
      work_++;
      work_.notify_all();
      schedulable_.wait( 0 );
    }
  }

public:
  // Return reference to static runtime storage
  static RuntimeStorage& get_instance()
  {
    static RuntimeStorage runtime_instance;
    return runtime_instance;
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

  // Get the total storage size downstream of this name
  size_t get_total_size( Handle name );

  // Get dependees. A list of tasks that must complete before this task may be unblocked
  std::vector<Task> get_dependees( Task depender ) { return fix_cache_.get_dependees( depender ); }
  // Get dependees
  std::optional<Handle> get_status( Task task ) { return fix_cache_.get( task ); };

  // get entries in local storage for debugging purposes
  Object& local_storage_at( size_t index ) { return local_storage_.at( index ); }

  size_t get_local_storage_size() { return local_storage_.size(); }

  Handle get_local_handle( Handle canonical ) { return canonical_to_local_.at( canonical ); }

  // Get the parent map of handle to tasks that produced handle. Potentially expensive operation
  // Linear in size of fixcache
  absl::flat_hash_map<Handle, std::vector<Task>, absl::Hash<Handle>> get_parent_map()
  {
    return fix_cache_.get_parent_map();
  }
};
