#pragma once

#include <condition_variable>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "ccompiler.hh"
#include "concurrent_storage.hh"
#include "concurrent_vector.hh"
#include "entry.hh"
#include "fixcache.hh"
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
  friend class Job;

  fixcache fix_cache_;

  // Maps a Wasm function Name to corresponding compiled Program
  InMemoryStorage<Program> name_to_program_;

  // Storage for Object/Names with a local name
  concurrent_vector<Object> local_storage_;

  std::vector<std::unique_ptr<RuntimeWorker>> workers_;

  size_t num_workers_;

  std::atomic<bool> threads_active_;

  std::atomic<size_t> work_;

  RuntimeStorage()
    : fix_cache_()
    , name_to_program_()
    , local_storage_()
    , workers_()
    , num_workers_( 16 )
    , threads_active_( false )
    , work_( 0 )
  {
    wasm_rt_init();
    llvm_init();

    for ( size_t i = 0; i < num_workers_; ++i ) {
      std::unique_ptr<RuntimeWorker> worker = std::make_unique<RuntimeWorker>( i, *this );
      workers_.push_back( std::move( worker ) );
    }

    threads_active_ = true;
    threads_active_.notify_one();
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
  Name add_blob( Blob&& blob );

  // Return reference to blob content
  std::string_view get_blob( Name name );
  std::string_view user_get_blob( const Name& name );

  // add Tree
  Name add_tree( Tree&& tree );

  // Return reference to Tree
  span_view<Name> get_tree( Name name );

  ObjectOrName& get_tree_obj( Name name );

  // add Thunk
  Name add_thunk( Thunk thunk );

  // Blocking force operations
  Name force_thunk( Name name );

  // Return encode name referred to by thunk
  Name get_thunk_encode_name( Name thunk_name );

  // Populate a program
  void populate_program( Name function_name );

  Name local_to_storage( Name name );

  std::string serialize( Name name );

  void deserialize();
};
