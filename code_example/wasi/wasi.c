#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "api.h"

extern void attach_input(uint32_t input_index, uint32_t mem_index);
extern uint32_t get_int(uint32_t mem_index, uint32_t ofst);
extern uint8_t get_int8(uint32_t mem_index, uint32_t ofst);

extern void mem_store(uint32_t offset, uint32_t content);
extern uint32_t mem_load(uint32_t offset);
extern void mem_store8(uint32_t offset, uint8_t content);
extern uint8_t mem_load8(uint32_t offset);

extern uint32_t mem_copy(uint32_t mem_index, uint32_t ofst, uint8_t * iov_buf, size_t iovs_len );

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
  size_t length_read = mem_copy( fd, fd_ofst[fd], iovs->buf, iovs_len );
  fd_ofst[fd] += length_read;
  return length_read;
}

int main() 
{
  mem_store(0,0);
  mem_load(0);
  mem_store8(0,0);
  mem_load8(0);
  return 0;
}
