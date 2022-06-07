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
  absl::flat_hash_map<Name, Name> memoization_cache;

  // Trace Cache: maps a Name to a human-readable string
  absl::flat_hash_map<Name, std::string> trace_cache;

  // Maps a string name to corresponding program
  absl::flat_hash_map<std::string, Program> name_to_program_;

  // Stores literal name that are requested
  std::vector<Name> literal_cache;

  // Unique id for local name
  uint32_t next_local_name_;

  RuntimeStorage()
    : storage()
    , memoization_cache()
    , trace_cache()
    , name_to_program_()
    , literal_cache()
    , next_local_name_( 0 )
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
  Name add_local_blob( Blob&& blob );

  // Return reference to blob content
  std::string_view get_blob( Name name );
  std::string_view user_get_blob( const Name& name );

  // add Tree
  Name add_tree( Tree&& tree );
  Name add_local_tree( Tree&& tree );

  // Return reference to Tree
  span_view<Name> get_tree( Name name );

  // add Thunk
  Name add_thunk( Thunk thunk );

  // Return encode name referred to by thunk
  Name get_thunk_encode_name( Name thunk_name );

  // add wasm module
  void add_wasm( const std::string& name, const std::string_view wasm_content );

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
};
