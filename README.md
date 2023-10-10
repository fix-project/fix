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
3. Build bootstrap as specified in `fix/docs/bootstrap.md`

4. Now, you should have a working `wasm-toolchain` in your `$HOME` directory,
   and `.fix` under your fix source directory. You are ready to build
Fix now:
```
cd ~/fix
cmake -S . -B build
cmake --build build/ --parallel 256
```

# How to run tests:
Fix contains a set of test cases. To run them:
```
cmake --build build/ --target test
```
`etc/tests.cmake` contains the location of test files.


# Run Wasm modules in Fix
The runtime of Fix accepts ELFs compiled from Wasm modules by a trusted compilation
toolchain as valid inputs of procedures. The name of the trusted compilation toolchain 
can be found at `.fix/refs/compile-encode`. 

`thunk: tree:3 string:none ref:compile-encode file:{path to wasm file}`
evaluates to the corresponding ELF in the required format. You can evaluate it directly in
stateless-tester:
```
./build/src/tester/stateless_tester tree:3 string:none ref:compile-encode file:{path to the Wasm module}
```
or, equivalently:
```
./build/src/tester/stateless_tester tree:3 string:none name:{contents of .fix/refs/compile-encode} file:{path to the Wasm module}
```
or replace the procedure of an ENCODE with the specification to run an ENCODE.

## Using `stateless_tester`
To run wasm files, the command is `./build/src/tester/stateless-tester` followed by a tree in the following format:
```
tree:4
├─ string:"0"
├─ thunk:
|  ├─ tree:3
|  |  ├─ string:unused
|  |  ├─ name:$COMPILE_NAME
|  |  ├─ file:$PATH_TO_WASM_FILE
├─ $ARGUMENT_1
├─ $ARGUMENT_2
```

where `$COMPILE_NAME` is the string contents of `.fix/refs/compile-encode`, e.g.

```
COMPILE_NAME=$(cat .fix/refs/compile-encode)
```

### Running Wasm Examples:
1. without arguments
```
./build/src/tester/stateless-tester tree:2 string:unused thunk: tree:3 string:unused name:$COMPILE_NAME file:build/applications-prefix/src/applications-build/flatware/examples/helloworld/helloworld-fixpoint.wasm
```
2. with reading from a directory using `SERIALIZED_HOME_DIRECTORY=$(cat build/file.txt)`
```
./build/src/tester/stateless-tester tree:4 string:unused thunk: tree:3 string:unused name:$COMPILE_NAME file:build/applications-prefix/src/applications-build/flatware/examples/open/open-deep-fixpoint.wasm tree:unused name:$SERIALIZED_HOME_DIRECTORY
```
3. with arguments
```
./build/src/tester/stateless-tester tree:4 string:unused thunk: tree:3 string:unused name:$COMPILE_NAME file:build/testing/wasm-examples/add-simple.wasm uint32:9 uint32:7
```

# Useful commands:
To figure out the serialized home directory run `./build/src/tester/serialization_test_deep`

# Fix Repo Structure

The `.fix` directory has the following structure (similar to `.git`):
```
.fix
├─ objects
|  ├─ <base64-encoded name>: contains a list of base64-encoded handles
|  └─ [...]
├─ refs
|  ├─ <human-readable name>: contains single base64-encoded handle
|  └─ [...]
```

The `refs` directory is, strictly speaking, optional; it's possible (and
arguably better) to specify everything using content-addressed Fix names.
However, it's pragmatically useful when debugging a Fix computation to be able
to match specific Fix names with a semantically-meaningful string (and even
just to know what Fix objects are semantically meaningful to the user).
