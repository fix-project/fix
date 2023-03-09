#pragma once

#include <condition_variable>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "ccompiler.hh"
#include "concurrent_flat_hash_map.hh"
#include "concurrent_queue.hh"
#include "concurrent_storage.hh"
#include "concurrent_vector.hh"
#include "name.hh"
#include "object.hh"
#include "program.hh"
#include "spans.hh"
#include "wasmcompiler.hh"

#include "absl/container/flat_hash_map.h"

class RuntimeStorage
{
private:
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

  concurrent_queue<std::tuple<Name, Name*, std::atomic<size_t>*>> ready_evaluate_;

  std::vector<std::thread> threads_;

  size_t num_threads_;

  bool threads_active_;

  std::condition_variable to_workers_;
  std::condition_variable to_producer_;

  std::mutex to_workers_mutex_;
  std::mutex to_producer_mutex_;

  RuntimeStorage()
    : storage()
    , memoization_cache()
    , trace_cache()
    , name_to_program_()
    , next_local_name_( 0 )
    , local_storage_()
    , ready_evaluate_()
    , threads_()
    , num_threads_( 1 )
    , threads_active_( true )
    , to_workers_()
    , to_producer_()
    , to_workers_mutex_()
    , to_producer_mutex_()
  {
    wasm_rt_init();
    llvm_init();

    threads_.resize( num_threads_ );

    for ( size_t i = 0; i < num_threads_; ++i ) {
      threads_.at( i ) = std::thread( &RuntimeStorage::worker, this );
    }
  }

  ~RuntimeStorage()
  {
    threads_active_ = false;
    to_workers_.notify_all();

    for ( size_t i = 0; i < num_threads_; ++i ) {
      threads_[i].join();
    }
  }

public:
  // Return reference to static runtime storage
  static RuntimeStorage& get_instance()
  {
    static RuntimeStorage runtime_instance;
    return runtime_instance;
  }

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

  // Return encode name referred to by thunk
  Name get_thunk_encode_name( Name thunk_name );

  // force the object refered to by a name
  Name force( Name name );

  void compute_job( std::tuple<Name, Name*, std::atomic<size_t>*> job );

  // Thread execution
  void worker();

  // force a Tree
  Name force_tree( Name tree_name );

  // force a Thunk
  Name force_thunk( Name thunk_name );

  // Reduce a Thunk by one step
  Name reduce_thunk( Name thunk_name );

  // Evaluate an encode
  Name evaluate_encode( Name encode_name );

  // Populate a program
  void populate_program( Name function_name );

  Name local_to_storage( Name name );

  std::string serialize( Name name );

  void deserialize();
};
