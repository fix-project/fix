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

4. Now, you should have a working wasm-toolchain`in your `{HOME}` directory,
and `boot` and `.fix` under your fix source directory. You are ready to build
Fix now:
```
cd ~/fix
cmake -S . -B build
cmake --build build/ --parallel 256
```

# How to run tests:
Fix contains a set of test cases. To run them:
```
cmake --build build/ --target fixpoint-check
```
`etc/tests.cmake` contains the location of test scripts and files


# Run Wasm modules in Fix
The runtime of Fix accepts ELFs compiled from Wasm modules by a trusted compilation
toolchain as valid inputs of procedures. The name of the trusted compilation toolchain
is can be found at `boot/compile-encode`.

`thunk: tree:3 string:none name:{content of boot/compile-encode} file:{content of boot/compile-encode}`
evaluates to the corresponding ELF in the required format. You can evaluate it directly in
stateless-tester:
```
./build/src/tester/stateless_tester tree:3 string:none name:{content of boot/compile-encode} file:{path to the Wasm module}
```
, or replace the procedure of an ENCODE with the specification to run an ENCODE.

# Using `wasi_tester`
To run wasm files the command is `./build/src/tester/wasi_tester` followed by the Wasm file you want to run.
Examples:
1. to run a file that has no arguments
```
./build/src/tester/wasi_tester build/flatware-prefix/src/flatware-build/examples/helloworld/helloworld-fixpoint.wasm
```
2. to run file that reads from a directory use -h flag
```
./build/src/tester/wasi_tester build/flatware-prefix/src/flatware-build/examples/open/open-deep-fixpoint.wasm -h <serialized home directory>
```
3. to run a file that takes in arguments pass them in while running
```
./build/src/tester/wasi_tester build/flatware-prefix/src/flatware-build/examples/add/add-fixpoint.wasm 1 2
```

# Useful commands:
To figure out the serialized home directory run `./build/src/tester/serialization_test_deep`
