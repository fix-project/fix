#High level overview:
Fixpoint is an operating system designed to allow users to be able to track a computation of a program and its answer without significant overhead.
To do this, fixpoint was designed to have the following principles of computational-centric networking:
1. Fine-grained visibility into application dataflow
2. An objective, common notion of correctness
3. A separation between I/O and compute, with delineated nondeterminism

To represent the first point we use abstractions of blob for a vector of bytes, thunk for a computation, and tree for a vector of names.

To ensure deterministic behavior we convert a user program to webassembly since it is a deterministic with a few exceptions. The code that does this lives in the flatware directory. The code that convert to the abstractions of fixpoint api lives in the src directory.

To learn more, please read refer to the design doc: https://github.com/fix-project/fixpoint-doc/

#Getting access to the machine:
First you need access to the research group's machines. Please get access from someone in the group by giving them your desired username and a public key. To generate a key run `ssh-keygen -t rsa` and follow instructions of the prompts. Your key will be added to a folder ~/.ssh.

How to set up your environment:
1. ssh into the research machine using `ssh -i <username>@stagecast.org`
2. clone fixpoint and the wasm-toolchain in the home directory by running:
    `git clone https://github.com/fix-project/fix.git`
    `git clone https://github.com/fix-project/wasm-toolchain.git`
3. Build the wasm-toolchain using the following commands:
    `cd wasm-toolchain`
    `git submodule update --init --recursive` #pulls dependent libraries
    `./build.sh build toolchain` #makes compiler c -> webassembly
4. Build fixpoint
    `cd ~/fix`
    `cmake -S . -B build` #creates build subdirectory snd sets source as fix directory
    `cmake --build build/ --parallel 256` #generates build in parallel using 256 cores

#How to run tests:

To see some user based tests look at ~/fix/build/flatware-prefix/src/flatware-build/examples/

To run wasm files the command is `./build/src/tester/wasi_tester` followed by the wasi file you want to run.
Examples:
to run a file that has no arguments
`./build/src/tester/wasi_tester build/flatware-prefix/src/flatware-build/examples/helloworld/helloworld-fixpoint.wasm`
to run file that reads from a directory use -h flag
`./build/src/tester/wasi_tester build/flatware-prefix/src/flatware-build/examples/open/open-deep-fixpoint.wasm -h <serialized home directory>`
to run a file that takes in arguments pass them in while running
`./build/src/tester/wasi_tester build/flatware-prefix/src/flatware-build/examples/add/add-fixpoint.wasm 1 2`

#Useful commands:
To figure out the serialized home directory run `./build/src/tester/serialization_test_deep`
