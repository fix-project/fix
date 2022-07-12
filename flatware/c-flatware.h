/**
 * THIS FILE IS AUTO-GENERATED from the following files:
 *   wasi_snapshot_preview1.witx
 *
 * To regenerate this file execute:
 *
 *     cargo run --manifest-path tools/wasi-headers/Cargo.toml generate-flatware
 *
 * Modifications to this file will cause CI to fail, the code generator tool
 * must be modified to change this file.
 */

#include <stdint.h>

int32_t args_get(int32_t argv, int32_t argv_buf) __attribute__((
    __export_name__("args_get")
));

int32_t args_sizes_get(int32_t retptr0, int32_t retptr1) __attribute__((
    __export_name__("args_sizes_get")
));

int32_t environ_get(int32_t environ, int32_t environ_buf) __attribute__((
    __export_name__("environ_get")
));

int32_t environ_sizes_get(int32_t retptr0, int32_t retptr1) __attribute__((
    __export_name__("environ_sizes_get")
));

int32_t clock_res_get(int32_t id, int32_t retptr0) __attribute__((
    __export_name__("clock_res_get")
));

int32_t clock_time_get(int32_t id, int64_t precision, int32_t retptr0) __attribute__((
    __export_name__("clock_time_get")
));

int32_t fd_advise(int32_t fd, int64_t offset, int64_t len, int32_t advice) __attribute__((
    __export_name__("fd_advise")
));

int32_t fd_allocate(int32_t fd, int64_t offset, int64_t len) __attribute__((
    __export_name__("fd_allocate")
));

int32_t fd_close(int32_t fd) __attribute__((
    __export_name__("fd_close")
));

int32_t fd_datasync(int32_t fd) __attribute__((
    __export_name__("fd_datasync")
));

int32_t fd_fdstat_get(int32_t fd, int32_t retptr0) __attribute__((
    __export_name__("fd_fdstat_get")
));

int32_t fd_fdstat_set_flags(int32_t fd, int32_t flags) __attribute__((
    __export_name__("fd_fdstat_set_flags")
));

int32_t fd_fdstat_set_rights(int32_t fd, int64_t fs_rights_base, int64_t fs_rights_inheriting) __attribute__((
    __export_name__("fd_fdstat_set_rights")
));

int32_t fd_filestat_get(int32_t fd, int32_t retptr0) __attribute__((
    __export_name__("fd_filestat_get")
));

int32_t fd_filestat_set_size(int32_t fd, int64_t size) __attribute__((
    __export_name__("fd_filestat_set_size")
));

int32_t fd_filestat_set_times(int32_t fd, int64_t atim, int64_t mtim, int32_t fst_flags) __attribute__((
    __export_name__("fd_filestat_set_times")
));

int32_t fd_pread(int32_t fd, int32_t iovs, int32_t iovs_len, int64_t offset, int32_t retptr0) __attribute__((
    __export_name__("fd_pread")
));

int32_t fd_prestat_get(int32_t fd, int32_t retptr0) __attribute__((
    __export_name__("fd_prestat_get")
));

int32_t fd_prestat_dir_name(int32_t fd, int32_t path, int32_t path_len) __attribute__((
    __export_name__("fd_prestat_dir_name")
));

int32_t fd_pwrite(int32_t fd, int32_t iovs, int32_t iovs_len, int64_t offset, int32_t retptr0) __attribute__((
    __export_name__("fd_pwrite")
));

int32_t fd_read(int32_t fd, int32_t iovs, int32_t iovs_len, int32_t retptr0) __attribute__((
    __export_name__("fd_read")
));

int32_t fd_readdir(int32_t fd, int32_t buf, int32_t buf_len, int64_t cookie, int32_t retptr0) __attribute__((
    __export_name__("fd_readdir")
));

int32_t fd_renumber(int32_t fd, int32_t to) __attribute__((
    __export_name__("fd_renumber")
));

int32_t fd_seek(int32_t fd, int64_t offset, int32_t whence, int32_t retptr0) __attribute__((
    __export_name__("fd_seek")
));

int32_t fd_sync(int32_t fd) __attribute__((
    __export_name__("fd_sync")
));

int32_t fd_tell(int32_t fd, int32_t retptr0) __attribute__((
    __export_name__("fd_tell")
));

int32_t fd_write(int32_t fd, int32_t iovs, int32_t iovs_len, int32_t retptr0) __attribute__((
    __export_name__("fd_write")
));

int32_t path_create_directory(int32_t fd, int32_t path, int32_t path_len) __attribute__((
    __export_name__("path_create_directory")
));

int32_t path_filestat_get(int32_t fd, int32_t flags, int32_t path, int32_t path_len, int32_t retptr0) __attribute__((
    __export_name__("path_filestat_get")
));

int32_t path_filestat_set_times(int32_t fd, int32_t flags, int32_t path, int32_t path_len, int64_t atim, int64_t mtim, int32_t fst_flags) __attribute__((
    __export_name__("path_filestat_set_times")
));

int32_t path_link(int32_t old_fd, int32_t old_flags, int32_t old_path, int32_t old_path_len, int32_t new_fd, int32_t new_path, int32_t new_path_len) __attribute__((
    __export_name__("path_link")
));

int32_t path_open(int32_t fd, int32_t dirflags, int32_t path, int32_t path_len, int32_t oflags, int64_t fs_rights_base, int64_t fs_rights_inheriting, int32_t fdflags, int32_t retptr0) __attribute__((
    __export_name__("path_open")
));

int32_t path_readlink(int32_t fd, int32_t path, int32_t path_len, int32_t buf, int32_t buf_len, int32_t retptr0) __attribute__((
    __export_name__("path_readlink")
));

int32_t path_remove_directory(int32_t fd, int32_t path, int32_t path_len) __attribute__((
    __export_name__("path_remove_directory")
));

int32_t path_rename(int32_t fd, int32_t old_path, int32_t old_path_len, int32_t new_fd, int32_t new_path, int32_t new_path_len) __attribute__((
    __export_name__("path_rename")
));

int32_t path_symlink(int32_t old_path, int32_t old_path_len, int32_t fd, int32_t new_path, int32_t new_path_len) __attribute__((
    __export_name__("path_symlink")
));

int32_t path_unlink_file(int32_t fd, int32_t path, int32_t path_len) __attribute__((
    __export_name__("path_unlink_file")
));

int32_t poll_oneoff(int32_t in, int32_t out, int32_t nsubscriptions, int32_t retptr0) __attribute__((
    __export_name__("poll_oneoff")
));

_Noreturn void proc_exit(int32_t rval) __attribute__((
    __export_name__("proc_exit")
));

int32_t proc_raise(int32_t sig) __attribute__((
    __export_name__("proc_raise")
));

int32_t sched_yield( void ) __attribute__((
    __export_name__("sched_yield")
));

int32_t random_get(int32_t buf, int32_t buf_len) __attribute__((
    __export_name__("random_get")
));

int32_t sock_accept(int32_t fd, int32_t flags, int32_t retptr0) __attribute__((
    __export_name__("sock_accept")
));

int32_t sock_recv(int32_t fd, int32_t ri_data, int32_t ri_data_len, int32_t ri_flags, int32_t retptr0, int32_t retptr1) __attribute__((
    __export_name__("sock_recv")
));

int32_t sock_send(int32_t fd, int32_t si_data, int32_t si_data_len, int32_t si_flags, int32_t retptr0) __attribute__((
    __export_name__("sock_send")
));

int32_t sock_shutdown(int32_t fd, int32_t how) __attribute__((
    __export_name__("sock_shutdown")
));

