#!/usr/bin/env bash

TEMP=$(mktemp -d)
pushd $TEMP
cp -r ~/bootstrap/.fix .
pushd ~/fix
cmake --build build --parallel
popd
fix add ~/fix/build/applications-prefix/src/applications-build/mapreduce/mapreduce.wasm -l mapreduce-wasm
fix add ~/fix/build/applications-prefix/src/applications-build/count-words/count_words.wasm -l count-words-wasm
fix add ~/fix/build/applications-prefix/src/applications-build/count-words/merge_counts.wasm -l merge-counts-wasm
fix eval compile: label:mapreduce-wasm
fix eval compile: label:count-words-wasm
fix eval compile: label:merge-counts-wasm
