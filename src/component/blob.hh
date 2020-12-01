#include <string>

#include "file_descriptor.hh"
#include "parser.hh"
#include "spans.hh"

class Blob {
  private:
    // Name of the blob
    std::string name_;
    // Content of the blob
    std::string content_;
    // Corresponding file descriptor
    int fd_;
    // File descriptor wrapper
    FileDescriptor fd_wrapper_;

    // TODO: If input are read-only, and only one output exists for a pair of program and input, there should not be concurrency issue for having one memory_instance. May become an issue if executions can take place on multiple cores in parallel???
    
    // Whether cashed in memory
    bool memory_cached_;

  public: 
    // Constructor
    Blob() {};
    Blob( std::string name ); 

    // Flush the file and computation tree 
    void flush();

    // Load the file into memory
    void load_to_memory();

    uint8_t serialized_length() const;
    void serialize( Serializer& s ) const;
    void parse( Parser& p );
} 


