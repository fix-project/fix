#pragma once

#include <condition_variable>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "ccompiler.hh"
#include "concurrent_flat_hash_map.hh"
#include "concurrent_storage.hh"
#include "concurrent_vector.hh"
#include "name.hh"
#include "object.hh"
#include "program.hh"
#include "spans.hh"
#include "wasmcompiler.hh"
#include "worker.hh"

#include "absl/container/flat_hash_map.h"

class RuntimeStorage
{
private:
  friend class RuntimeWorker;

  // Storage: maps a Name to the contents of the corresponding Object
  InMemoryStorage<Object> storage;

  // Memorization Cache: maps a Thunk to the Name of a reduction of the original Thunk
  concurrent_flat_hash_map<Name> memoization_cache;

  // Trace Cache: maps a Name to a human-readable string
  concurrent_flat_hash_map<std::string> trace_cache;

  // Maps a Wasm function Name to corresponding compiled Program
  concurrent_flat_hash_map<Program> name_to_program_;

  // Unique id for local name
  size_t next_local_name_;

  // Storage for Object/Names with a local name
  concurrent_vector<ObjectOrName> local_storage_;

  std::vector<std::unique_ptr<RuntimeWorker>> workers_;

  size_t num_workers_;

  std::condition_variable to_workers_;

  std::mutex to_workers_mutex_;

  bool threads_active_;

  RuntimeStorage()
    : storage()
    , memoization_cache()
    , trace_cache()
    , name_to_program_()
    , next_local_name_( 0 )
    , local_storage_()
    , workers_()
    , num_workers_( 16 )
    , to_workers_()
    , to_workers_mutex_()
    , threads_active_( false )
  {
    wasm_rt_init();
    llvm_init();

    for ( size_t i = 0; i <= num_workers_; ++i ) {
      std::unique_ptr<RuntimeWorker> worker = std::make_unique<RuntimeWorker>( i, *this );
      workers_.push_back( std::move( worker ) );
    }

    threads_active_ = true;
    to_workers_.notify_all();
  }

  ~RuntimeStorage()
  {
    threads_active_ = false;
    to_workers_.notify_all();

    for ( size_t i = 1; i <= num_workers_; ++i ) {
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

  Name entry_force_thunk( Name name );

  // add blob
  Name add_blob( Blob&& blob );

  // Return reference to blob content
  std::string_view get_blob( Name name );
  std::string_view user_get_blob( const Name& name );

  // add Tree
  Name add_tree( Tree&& tree );

  // Return reference to Tree
  span_view<Name> get_tree( Name name );

  // add Thunk
  Name add_thunk( Thunk thunk );

  Name force_thunk( Name name );

  // Return encode name referred to by thunk
  Name get_thunk_encode_name( Name thunk_name );

  // Populate a program
  void populate_program( Name function_name );

  Name local_to_storage( Name name );

  std::string serialize( Name name );

  void deserialize();
};
