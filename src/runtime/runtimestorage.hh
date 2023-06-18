#pragma once

#include <condition_variable>
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
  friend struct Job;

  fixcache fix_cache_;

  // Maps a Wasm function Handle to corresponding compiled Program
  InMemoryStorage<Program> name_to_program_;

  // Storage for Object/Handles with a local name
  concurrent_vector<Object> local_storage_;

  std::vector<std::unique_ptr<RuntimeWorker>> workers_;

  size_t num_workers_;

  std::atomic<bool> threads_active_;
  std::atomic<bool> threads_started_;

  std::atomic<size_t> work_;

  inline static thread_local Handle current_procedure_;

  RuntimeStorage()
    : fix_cache_()
    , name_to_program_()
    , local_storage_()
    , workers_()
    , num_workers_( 16 )
    , threads_active_( true )
    , threads_started_( false )
    , work_( 0 )
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

public:
  // Return reference to static runtime storage
  static RuntimeStorage& get_instance()
  {
    static RuntimeStorage runtime_instance;
    return runtime_instance;
  }

  bool steal_work( Job& job, size_t tid );

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
  void deserialize();

  void set_current_procedure( const Handle function_name ) { current_procedure_ = function_name; }

  Handle get_current_procedure() const { return current_procedure_; }
};
