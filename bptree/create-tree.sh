#!/usr/bin/env bash

rm -r .fix
rm keys
rm keys-in-tree
rm keys-not-in-tree

cp -r ~/fixpoint/.fix . -r
rm .fix/data/*
../build/src/tester/fix label $(../build/src/tests/create-bptree 4 1048576 keys 10000 keys-in-tree keys-not-in-tree) tree-root
