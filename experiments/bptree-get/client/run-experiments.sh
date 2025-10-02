#!/usr/bin/env bash

bptree_roots=(4096 256 64)
bptree_style=(good)

for r in "${bptree_roots[@]}"
do
	for s in "${bptree_style[@]}"
	do
		for ((i=0; i<17; i=i+1))
		do
			./run.sh ~/fix/build ../server . wikipedia-samples 400 bptree-get-$s bptree-root-$r > fix-run-$r-$s-${i}
		done
	done
done

bptree_large_roots=(16777216)
	
for r in "${bptree_large_roots[@]}"
do
	for s in "${bptree_style[@]}"
	do
		for ((i=0; i<3; i=i+1))
		do
			./run.sh ~/fix/build ../server . wikipedia-samples 400 bptree-get-$s bptree-root-$r > fix-run-$r-$s-${i}
		done
	done
done
