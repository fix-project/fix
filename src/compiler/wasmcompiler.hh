#include <utility>
#include <string>
#include <string_view>

#include "src/stream.h"

namespace wasmcompiler 
{
  class MemoryStringStream : public wabt::Stream
  {
    public:
      WABT_DISALLOW_COPY_AND_ASSIGN( MemoryStringStream )
      explicit MemoryStringStream( Stream* log_stream = nullptr );

      std::string && ReleaseStringBuf();
      std::string_view Buf() { return { buf_ }; }
    protected:
      wabt::Result WriteDataImpl(size_t offset, const void* data, size_t size) override;
      wabt::Result MoveDataImpl(size_t dst_offset,
          size_t src_offset,
          size_t size) override;
      wabt::Result TruncateImpl(size_t size) override;

    private:
      std::string buf_{};
  };
  
  std::pair<std::string, std::string> wasm_to_c( const std::string & wasm_name, const std::string & wasm_content );

}
