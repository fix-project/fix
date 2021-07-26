#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "api.h"

extern void attach_input(uint32_t input_index, uint32_t mem_index);
extern uint32_t get_int(uint32_t mem_index, uint32_t ofst);
extern uint8_t get_int8(uint32_t mem_index, uint32_t ofst);

extern uint8_t mem_store(uint32_t offset, uint8_t content);
extern uint8_t mem_load(uint32_t offset, uint8_t content);

uint32_t fd_ofst [5];
uint32_t next_fd_idx = 0;

__wasi_errno_t __my_wasi_path_open(
    __wasi_fd_t fd,
    /**
     * Flags determining the method of how the path is resolved.
     */
    __wasi_lookupflags_t dirflags,
    /**
     * The relative path of the file or directory to open, relative to the
     * `path_open::fd` directory.
     */
    const char *path,
    /**
     * The method by which to open the file.
     */
    __wasi_oflags_t oflags,
    /**
     * The initial rights of the newly created file descriptor. The
     * implementation is allowed to return a file descriptor with fewer rights
     * than specified, if and only if those rights do not apply to the type of
     * file being opened.
     * The *base* rights are rights that will apply to operations using the file
     * descriptor itself, while the *inheriting* rights are rights that apply to
     * file descriptors derived from it.
     */
    __wasi_rights_t fs_rights_base,
    __wasi_rights_t fs_rights_inheriting,
    __wasi_fdflags_t fdflags,
    __wasi_fd_t *retptr0
) 
{
  uint32_t input_index = (uint32_t)(atoi(path));
  attach_input(input_index, next_fd_idx);
  next_fd_idx += 1;
  return next_fd_idx;
}

__wasi_errno_t __my_wasi_fd_read(
    __wasi_fd_t fd,
    /**
     * List of scatter/gather vectors to which to store data.
     */
    const __wasi_iovec_t *iovs,
    /**
     * The length of the array pointed to by `iovs`.
     */
    size_t iovs_len,
    __wasi_size_t *retptr0
)__attribute__((export_name("my_wasi_fd_read")))
{
  for( int i = 0; i < iovs_len; i++ )
  {
    iovs->buf[i] = get_int8(fd, fd_ofst[fd]);
    fd_ofst[fd]++;
  }
  return iovs_len;  
}

int main() 
{
  return 0;
}
