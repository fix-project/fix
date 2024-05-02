#!/usr/bin/env bash

TEMP=$(mktemp -d)
pushd $TEMP
tar -xzf /usr/local/share/bootstrap/bootstrap.tar.gz
pushd ~/fix
cmake --build build --parallel
popd
fix add ~/fix/build/applications-prefix/src/applications-build/mapreduce/mapreduce.wasm -l mapreduce-wasm > /dev/null
fix add ~/fix/build/applications-prefix/src/applications-build/count-words/count_words.wasm -l count-words-wasm > /dev/null
fix add ~/fix/build/applications-prefix/src/applications-build/count-words/merge_counts.wasm -l merge-counts-wasm > /dev/null
fix label $(fix eval compile: label:mapreduce-wasm) mapreduce
fix label $(fix eval compile: label:count-words-wasm) count-words
fix label $(fix eval compile: label:merge-counts-wasm) merge-counts
popd
rm -rf repo-template
mv $TEMP/.fix repo-template
rm -rf $TEMP
