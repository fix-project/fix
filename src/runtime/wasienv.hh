#include <map>

#include "invocation.hh"

// ID of the current invocation
thread_local uint64_t invocation_id;

enum class fd_mode
{
  READ,
  WRITE
};

struct file_descriptor 
{
  std::string blob_name_;
  uint64_t loc;
  fd_mode mode;
} FileDescriptor;

class WasiEnvironment 
{
  private:
    // Map from fd id to actual fd
    std::map<uint64_t, FileDescriptor> id_to_fd_;

    // Map from invocation id to actual invocation
    std::map<uint64_t, Invocation> id_to_inv_;

  public:
    int path_open( std::string varaible_name ); 
    int fd_read( int fd_id, uint64_t ofst, uint64_t count );
    int fd_write( int fd_id, uint64_t ofst, uint64_t count );
}




