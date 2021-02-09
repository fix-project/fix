#pragma once

#include <map>
#include <string>
#include <string_view>
#include <unordered_map>

#include "blob.hh"
#include "encoded_blob.hh"
#include "encode.hh"
#include "program.hh"
#include "storage.hh"

#include "wasienv.hh"

class RuntimeStorage {
  private:
    // Map from name to content (blob / encoded blob)
    InMemoryStorage<Blob> name_to_blob_;
    // Map from name to content of encoded blob
    InMemoryStorage<EncodedBlob> name_to_encoded_blob_;
    // Map from name to encode
    InMemoryStorage<Encode> name_to_encode_;
    // Map from name to program
    InMemoryStorage<Program> name_to_program_;

    // Map from encoded blob name to blob name
    std::unordered_map<std::string, std::string> encoded_blob_to_blob_;

    RuntimeStorage ()
      : name_to_blob_(),
        name_to_encoded_blob_(),
        name_to_encode_(),
        name_to_program_(),
        encoded_blob_to_blob_()
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

    // add blob
    template<typename T>
    std::string addBlob( T&& content )
    {
      std::string blob_content ( reinterpret_cast<char *>( &content ), sizeof( T ) );
      Blob blob ( move( blob_content ) );
      std::string name = blob.name();
      name_to_blob_.put( name, std::move( blob ) );
      return name;
    }

    // add elf program
    void addProgram( std::string & name, std::vector<std::string> && inputs, std::vector<std::string> && outputs, std::string & program_content );

    // add encode
    std::string addEncode( const std::string & program_name, const std::vector<std::string> & input_blobs );

    // execute encode
    void executeEncode( const std::string & encode_name );
};
