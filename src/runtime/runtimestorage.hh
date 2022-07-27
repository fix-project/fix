#pragma once

#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

#include "ccompiler.hh"
#include "name.hh"
#include "object.hh"
#include "program.hh"
#include "spans.hh"
#include "storage.hh"
#include "wasmcompiler.hh"

#include "absl/container/flat_hash_map.h"

class RuntimeStorage
{
private:
  // Storage: maps a Name to the contents of the corresponding Object
  InMemoryStorage<Object> storage;

  // Memorization Cache: maps a Thunk to the Name of a reduction of the original Thunk
  absl::flat_hash_map<Name, Name, NameHash> memoization_cache;

  // Trace Cache: maps a Name to a human-readable string
  absl::flat_hash_map<Name, std::string, NameHash> trace_cache;

  // Maps a Wasm function Name to corresponding compiled Program
  absl::flat_hash_map<Name, Program, NameHash> name_to_program_;

  // Unique id for local name
  size_t next_local_name_;

  // Storage for Object/Names with a local name
  std::vector<ObjectOrName> local_storage_;

  // Number of instances
  size_t init_instances_;

  RuntimeStorage()
    : storage()
    , memoization_cache()
    , trace_cache()
    , name_to_program_()
    , next_local_name_( 0 )
    , local_storage_()
    , init_instances_( 16 )
  {
    wasm_rt_init();
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

  // force a Tree
  Name force_tree( Name tree_name );

  // force a Thunk
  Name force_thunk( Name thunk_name );

  // Reduce a Thunk by one step
  Name reduce_thunk( Name thunk_name );

  // Evaluate an encode
  Name evaluate_encode( Name encode_name );

  void set_init_instances( size_t init_instances ) { init_instances_ = init_instances; }
  size_t get_init_instances() const { return init_instances_; }

  // Populate a program
  void populate_program( Name function_name );

  Name local_to_storage( Name name );

  std::string serialize( Name name );

  void deserialize();
};
