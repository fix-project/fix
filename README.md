# Dependencies

- [CMake >=3.16](https://cmake.org/)
- [GCC >=12](https://gcc.gnu.org/) (C++23 support required)
- [Google Logging Library](https://github.com/google/glog)

# Environment Setup

1. clone fixpoint and the wasm-toolchain in the home directory by running:
```
git clone https://github.com/fix-project/fix.git
git clone https://github.com/fix-project/wasm-toolchain.git
```
2. Build the wasm-toolchain using the following commands:
```
cd wasm-toolchain
git submodule update --init --recursive
./build.sh
```
3. Build bootstrap as specified in [fix/docs/bootstrap.md](docs/bootstrap.md)

4. Now, you should have a working `wasm-toolchain` in your `$HOME` directory,
   and `.fix` under your fix source directory. You are ready to build
Fix now:
```
cd ~/fix
cmake -S . -B build
cmake --build build/ --parallel 256
```

# How to run tests:
Fix contains a set of test cases.
* unit-test: Unit tests for individual components of Fixpoint implementation
* fixpoint-check: End-to-end tests that execute functions from Wasm modules
* all-fixpoint-check: fixpoint-check + a test that verifies the trusted compilation toolchain is self-hosting
* flatware-check: End-to-end tests that execute functions written against POSIX and ported to Fixpoint via the Flatware library
* test: Include all tests above

To run them:
```
cmake --build build/ --target ${test-name}
```
`etc/tests.cmake` contains the location of test files.


# Run Wasm modules in Fix
The runtime of Fix accepts ELFs compiled from Wasm modules by a trusted compilation
toolchain as valid inputs of procedures. The name of the trusted compilation toolchain
can be found at `.fix/refs/compile-encode`.

`application: tree:3 string:none label:compile-encode file:{path to wasm file}`
evaluates to the corresponding ELF in the required format. You can evaluate it directly in
fix:
```
./build/src/tester/fix eval application: tree:3 tree:3 uint64:1000000000 uint64:1 uint64:1 label:compile-encode file:{path to the Wasm module}
```
or, equivalently:
```
./build/src/tester/fix eval application: tree:3 tree:3 uint64:1000000000 uint64:1 uint64:1 name:{contents of (readlink .fix/labels/compile-encode)} file:{path to the Wasm module}
```

## Using `fix eval`
To run wasm files, the command is `./build/src/tester/fix eval` followed by an Object.

It can be a Blob, ObjectTree or ValueTree, e.g.
```
tree:2
├─ uint32:1
├─ uint32:2
```

or an Application Thunk, e.g.
```
application:
├─ tree:4
|  ├─ tree:3
|  |  ├─ uint64:$ALLOWED_MEMORY
|  |  ├─ uint64:$ESTIMATED_OUTPUT_SIZE
|  |  ├─ uint64:$ESTIMATED_FANOUT
|  ├─ strict:
|  |  ├─ application:
|  |  |  ├─ tree:3
|  |  |  |  ├─ tree:3
|  |  |  |  |  ├─ uint64:$ALLOWED_MEMORY
|  |  |  |  |  ├─ uint64:$ESTIMATED_OUTPUT_SIZE
|  |  |  |  |  ├─ uint64:$ESTIMATED_FANOUT
|  |  |  |  ├─ name:$COMPILE_NAME
|  |  |  |  ├─ file:$PATH_TO_WASM_FILE
|  ├─ $ARGUMENT_1
|  ├─ $ARGUMENT_2
```

where `$COMPILE_NAME` is the name of the data that `.fix/refs/compile-encode` links to, e.g.

```
COMPILE_NAME=$(readelf .fix/refs/compile-encode | cut -c 9-)
```
This Application Thunk can be simplified as:
```
application:
├─ tree:4
|  ├─ tree:3
|  |  ├─ uint64:$ALLOWED_MEMORY
|  |  ├─ uint64:$ESTIMATED_OUTPUT_SIZE
|  |  ├─ uint64:$ESTIMATED_FANOUT
|  ├─ compile:
|  |  ├─ file:$PATH_TO_WASM_FILE
|  |  ├─ (or) name:$NAME_OF_WASM_BLOB
|  |  ├─ (or) label:$LABEL_OF_WASM_BLOB
|  ├─ $ARGUMENT_1
|  ├─ $ARGUMENT_2
```

### Running Wasm Examples:
1. without arguments
```
./build/src/tester/fix eval application: tree:2 tree:1 uint64:1000000 compile: file:build/applications-prefix/src/applications-build/flatware/examples/helloworld/helloworld-fixpoint.wasm
```
2. with reading from a directory using `SERIALIZED_HOME_DIRECTORY=$(cat build/file.txt)`
```
./build/src/tester/fix eval application: tree:4 tree:1 uint64:1000000 compile: file:build/applications-prefix/src/applications-build/flatware/examples/open/open-deep-fixpoint.wasm string:unused name:$SERIALIZED_HOME_DIRECTORY
```
3. with arguments
```
./build/src/tester/fix eval application: tree:4 tree:1 uint64:1000000 compile: file:build/testing/wasm-examples/add-simple.wasm uint32:9 uint32:7
```

# Fix Repo Structure

The `.fix` directory has the following structure (similar to `.git`):
```
.fix
├─ data
|  ├─ <base16-encoded name>: contains data of object
|  └─ [...]
├─ labels
|  ├─ <human-readable name>: symlink to an object in data
|  └─ [...]
├─ pins
├─ relations
```

# Experiment Artifacts
See [experiments/Experiment.md].
