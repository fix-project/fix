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
  absl::flat_hash_map<Name, Name> memorization_cache;

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
    , memorization_cache()
    , trace_cache()
    , name_to_program_()
    , literal_cache()
    , next_local_name_( 0 )
  {
  }

public:
  // Return reference to static runtime storage
  static RuntimeStorage& get_instance()
  {
    static RuntimeStorage runtime_instance;
    return runtime_instance;
  }

  // add blob
  Name add_blob( std::string&& content );
  Name add_local_blob( std::string&& content );

  // Return reference to blob content
  std::string_view get_blob( Name name );

  // add Tree
  Name add_tree( std::vector<Name>&& content );
  Name add_local_tree( std::vector<Name>&& content );

  // Return reference to Tree
  span_view<Name> get_tree( Name name );

  // add Thunk
  Name add_thunk( Thunk thunk );

  // Return encode name referred to by thunk
  Name get_thunk_encode_name( Name thunk_name );

  // add wasm module
  void add_wasm( const std::string& name, const std::string& wasm_content );

  // add elf program
  void add_program( const std::string& name, std::string& program_content );

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
