#!/usr/bin/env bash

mkdir -p repo-template
pushd ~/fixpoint
./build/src/tester/fix add build/applications-prefix/src/applications-build/bptree-get/bptree-get.wasm -l bptree-get-wasm > /dev/null
./build/src/tester/fix add build/applications-prefix/src/applications-build/bptree-get/bptree-get-n.wasm -l bptree-get-n-wasm > /dev/null
./build/src/tester/fix label $(./build/src/tester/fix eval compile: label:bptree-get-wasm) bptree-get 
./build/src/tester/fix label $(./build/src/tester/fix eval compile: label:bptree-get-n-wasm) bptree-get-n
popd
rm -rf repo-template
cp ~/fixpoint/.fix repo-template -r
