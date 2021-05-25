#pragma once

#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

#include "blob.hh"
#include "ccompiler.hh"
#include "name.hh"
#include "program.hh"
#include "spans.hh"
#include "storage.hh"
#include "thunk.hh"
#include "tree.hh"
#include "wasmcompiler.hh"

#include "absl/container/flat_hash_map.h"
#include "wasienv.hh"

class RuntimeStorage {
  private:
    friend class Invocation;

    // Map from name to Blob
    InMemoryStorage<Blob> name_to_blob_;
    // Map from name to Tree
    InMemoryStorage<Tree> name_to_tree_;
    // Map from name to Thunk
    InMemoryStorage<Thunk> name_to_thunk_;
    
    // Map from name to program
    absl::flat_hash_map<std::string, Program> name_to_program_;
    // Map from thunk name to blob name
    absl::flat_hash_map<Name, Name> thunk_to_blob_;
    
    std::vector<std::string> literal_name_store;

    RuntimeStorage ()
      : name_to_blob_(),
        name_to_tree_(),
        name_to_thunk_(),
        name_to_program_(),
        thunk_to_blob_(),
        literal_name_store()
    {}

  public:
    // Return reference to blob content
    std::string_view getBlob( const Name & name );

    // Return reference to static runtime storage
    static RuntimeStorage & getInstance () 
    {
      static RuntimeStorage runtime_instance;
      return runtime_instance;
    }

    // Return reference to Tree
    const Tree & getTree ( const Name & name );

    // add blob
    Name addBlob( std::string && content );

    // add Tree
    Name addTree( std::vector<Name> && content );

    // add Encode
    Name addEncode( const Name & program_name, const Name & strict_input, const Name & lazy_input );

    // add wasm module
    void addWasm( const std::string & name, const std::string & wasm_content, const std::vector<std::string> & deps );

    // add elf program
    void addProgram( const std::string & name, std::vector<std::string> && inputs, std::vector<std::string> && outputs, std::string & program_content );

    // force the object refered to by a name
    Name force( Name name );

    // force a Tree
    Name forceTree( Name tree_name );

    // force a Thunk
    Name forceThunk( const Thunk & thunk ); 

    // Evaluate an encode
    void evaluateEncode( Name encode_name );

    // execute encode
    // void executeEncode( const std::string & encode_name, int arg1, int arg2 );
};
