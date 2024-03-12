# Fixpoint Flatware

The Fixpoint flatware is an implementation of POSIX APIs that allows regular 
POSIX-compliant programs to be run on in Fixpoint. Flatware programs use an 
extension of the combination tree format as their input.

## Flatware combination format

The Flatware combination format is an extension of the Fixpoint combination. It
describes the application of a Flatware program to inputs, producing an Object 
as output. The first entry specifies the metadata, possibly including resource 
limits at runtime (e.g. maximum pages of mutable memory), and the format of the
combination. A combination format is either "apply" or "lift" (not yet 
implemented).

An "apply" combination may nest another combination within itself, thus allowing
for nested argument capture. 

For an "apply" combination, the second entry in the tree specifies the 
procedure, which is either:
  1. A Tag that the first entry is a Blob, the second entry is a trusted 
  compilation toolchain, and the third entry is "Runnable" or
  2. A combination

For a "lift" combination, the second entry is the Handle to the Object to be 
lifted.

The third entry contains the filesystem to be used by the program. See the
Flatware Filesystem section for more details.

The fourth entry is the entry arguments to the program. The arguments are
specified as a tree of null-terminated strings. The first entry is the program
name, and the remaining entries are the arguments to the program.

The fifth entry is STDIN. It is specified as a single null-terminated string 
blob.

The sixth entry is the environment variables. It is specified as a tree of
null-terminated strings. Each entry is of the form "NAME=VALUE".


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
