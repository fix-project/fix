#pragma once

#include <map>
#include <string>
#include <string_view>

#include "blob.hh"
#include "encoded_blob.hh"
#include "encode.hh"
#include "program.hh"
#include "storage.hh"

class RuntimeStorage {
  private:
    // Map from name to content (blob / encoded blob)
    InMemoryStorage<Blob> name_to_blob_;
    // Map from name to content of encoded blob
    InMemoryStorage<EncodedBlob> name_to_encoded_blob_;
    // Map from name to encode
    std::map<std::string, Encode> name_to_encode_;
    // Map from name to program
    std::map<std::string, Program> name_to_program_;

    RuntimeStorage ()
      : name_to_blob_(),
        name_to_encoded_blob_(),
        name_to_encode_(),
        name_to_program_() 
    {}

  public:
    // Return reference to blob content
    std::string_view getBlob( const std::string & name );

    // Return reference to encoded blob content
    std::string & getEncodedBlob( const std::string & name );

    static RuntimeStorage & getInstance () 
    {
      static RuntimeStorage runtime_instance;
      return runtime_instance;
    }

    // add
};
