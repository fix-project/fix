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
#include "wasienv.hh"

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

  RuntimeStorage()
    : storage()
    , memorization_cache()
    , trace_cache()
    , name_to_program_()
  {}

public:
  // Return reference to static runtime storage
  static RuntimeStorage& getInstance()
  {
    static RuntimeStorage runtime_instance;
    return runtime_instance;
  }

  // add blob
  Name addBlob( std::string&& content );

  // Return reference to blob content
  std::string_view getBlob( const Name& name );

  // add Tree
  Name addTree( std::vector<TreeEntry>&& content );

  // Return reference to Tree
  span_view<TreeEntry> getTree( const Name& name );

  // Return encode name referred to by thunk
  Name getThunkEncodeName( const Name& thunk_name );

  // add Encode
  Name addEncode( const Name& program_name, const Name& strict_input, const Name& lazy_input );
  Name addEncode( const Name& program_name, const Name& strict_input );

  // add wasm module
  void addWasm( const std::string& name, const std::string& wasm_content, const std::vector<std::string>& deps );

  // add elf program
  void addProgram( const std::string& name,
                   std::vector<std::string>&& inputs,
                   std::vector<std::string>&& outputs,
                   std::string& program_content );

  // force the object refered to by a name
  Name force( Name name );

  // force a Tree
  Name forceTree( Name tree_name );

  // force a Thunk
  Name forceThunk( Name thunk_name );

  // Reduce a Thunk by one step
  Name reduceThunk( Name thunk_name );

  // Evaluate an encode
  Name evaluateEncode( Name encode_name );
};
