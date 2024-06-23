# Fixpoint Flatware

The Fixpoint flatware is an implementation of POSIX APIs that allows regular 
POSIX-compliant programs to be run on in Fixpoint. Flatware programs use an 
extension of the combination tree format as their input.

## Flatware combination format

The Flatware combination format is an extension of the Fixpoint combination. It
describes the application of a Flatware program to inputs, producing an Object 
as output. The first entry specifies the metadata, possibly including resource 
limits at runtime (e.g. maximum pages of mutable memory), and the format of the
combination.

A combination may nest another combination within itself, allowing
for nested argument capture.

The second entry in the combination specifies the procedure, which is either:
  1. A Tag that the first entry is a Blob, the second entry is a trusted 
  compilation toolchain, and the third entry is "Runnable" or
  2. A combination

The third entry contains the filesystem to be used by the program. See the
Flatware Filesystem section for more details.

The fourth entry is the entry arguments to the program. The arguments are
specified as a tree of null-terminated strings. The first entry is the program
name, and the remaining entries are the arguments to the program.

The fifth entry is STDIN. It is specified as a single null-terminated string 
blob.

The sixth entry is the environment variables. It is specified as a tree of
null-terminated strings. Each entry is of the form "NAME=VALUE".

The seventh entry is a random seed. It is specified as a blob of 4 bytes.


```
"apply" combination example
tree:5
├─ string:unused
├─ tree:4
|  ├─ string:unused
|  ├─ tag
|  |  ├─ handle to TRUSTED_COMPILE
|  |  ├─ blob:EXECUTABLE
|  |  ├─ literal blob "Runnable"
├─ tree:3 (filesystem)
|  ├─ ...
├─ tree:2 (args)
|  ├─ ARGUMENT_1_1
|  ├─ ARGUMENT_1_2
├─ blob: STDIN
├─ tree:2 (env)
|  ├─ ENV_1_1
|  ├─ ENV_1_2
├─ blob:4 (random seed)
```

## Flatware Filesystem

The Flatware filesystem is a tree of dirents. The dirent is specified as
follows:

```
handle: dirent
├─ blob:name
├─ blob:permissions
├─ handle: contents
```

The dirent contents can be another dirent or file contents as a blob.

## Flatware Output

The result of a full apply on the Flatware combination is a tree. The first
entry is the exit code of the program. The second entry is the modified 
filesystem after the program has run. The third entry is the STDOUT of the 
program. The fourth entry is the STDERR of the program.

The fifth entry is an optional trace of the program, based on compile-time 
flags. The trace is a serialized list of all syscalls made by the program.

```
tree:4
├─ blob:exit code
├─ tree:3 (filesystem)
|  ├─ ...
├─ blob: STDOUT
├─ blob: STDERR
├─ blob: trace
```

## WASI Implementation Status

- [x] `proc_exit`
- [x] `fd_close`
- [x] `fd_fdstat_get`
- [x] `fd_seek`
- [x] `fd_read`
- [x] `fd_write`
- [x] `fd_fdstat_set_flags`
- [x] `fd_prestat_get`
- [x] `fd_prestat_dir_name`
- [ ] `fd_advise` (no effect)
- [x] `fd_allocate`
- [ ] `fd_datasync` (no effect)
- [x] `fd_filestat_get`
- [x] `fd_filestat_set_size`
- [x] `fd_filestat_set_times`
- [x] `fd_pread`
- [x] `fd_pwrite`
- [x] `fd_readdir`
- [ ] `fd_sync` (no effect)
- [x] `fd_tell`
- [x] `path_create_directory`
- [x] `path_filestat_get`
- [ ] `path_filestat_set_times`
- [ ] `path_link`
- [ ] `path_readlink`
- [ ] `path_remove_directory`
- [ ] `path_rename`
- [ ] `path_symlink`
- [ ] `path_unlink_file`
- [x] `arg_sizes_get`
- [x] `args_get`
- [x] `environ_sizes_get`
- [x] `environ_get`
- [ ] `path_open` (only supports existing files)
- [x] `clock_res_get` (throws invalid argument error for all clocks as time is not implemented)
- [ ] `clock_time_get` (note: will likely be implemented to return a constant from the input combination)
- [ ] `poll_oneoff`
- [ ] `sched_yield`
- [x] `random_get`
- [ ] `sock_accept`
- [ ] `sock_recv`
- [ ] `sock_send`
- [ ] `sock_shutdown`


