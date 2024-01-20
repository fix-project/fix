# Navigating Bootstrap
Bootstrap is a compilation toolchain in Wasm (on top of Fix) that compiles Wasm
modules (on top of Fix) to machine code (currently X86). Since Fix only executes
machine code, we need to compile Bootstrap itself to machine code before we can
execute in Fix. Given a Wasm module, Bootstrap runs `wasm2c` and `initcomposer`.
Then, it compiles each output c file using `clang`, and `lld` the results.

There are two ways to get the compilation toolchain.

## Way 0: Build from source
Clone the source from [bootstrap](https://github.com/fix-project/bootstrap).
Building bootstrap from source requires a working
[wasm-toolchain](https://github.com/fix-project/wasm-toolchain).
```
cmake -S . -B build
cmake --build build --parallel {# of parallelism}
cmake -S . -B fix-build -DBUILD_SYS_DRIVER=OFF
cmake --build fix-build --parallel {# of parallelism}
./build.sh
./serialize.sh
```
After building, copy `./.fix/` to the source directory of Fix.

## Way 1: Fetch from Release
Either
```
wget https://github.com/fix-project/bootstrap/releases/download/v0.5/bootstrap.tar.gz
```
or use `gh`
```
gh release download --pattern 'bootstrap.zip' --repo 'fix-project/bootstrap'
```

## Upgrade Bootstrap
The output of building bootstrap depends on two things:
* Which commit `wasm-toolchain` is at? (This includes which commit each
  submodule of `wasm-toolchain` is at.)
* The version that the kernel of the machine is at.
