#!/bin/bash

cd ~/ray-benchmark/local
./run-wiki-exps.sh

bptree_roots=(16777216 4096 256 64)
for r in "${bptree_roots[@]}"
do
	# Remove the data for warm-up pass
	rm -rf ray-run-${r}-good-0
	rm -rf ray-run-${r}-bad-0
	raycp_values=$(cat ray-run-"${r}"-good-* | awk '{print $1}' )
	raybl_values=$(cat ray-run-"${r}"-bad* | awk '{print $1}' )

	sum=0
	count=0

	for v in $raycp_values; do
		sum=$(echo "$sum + $v" | bc -l)
		count=$((count + 1))
	done

	if [ $count -gt 0 ]; then
		avg=$(echo "$sum / $count" | bc -l)
		echo "B+ Tree arity $r, Ray Continuation Passing: Average over $count runs: ${avg}s"
	else
		echo "No results found to average."
	fi

	sum=0
	count=0

	for v in $raybl_values; do
		sum=$(echo "$sum + $v" | bc -l)
		count=$((count + 1))
	done

	if [ $count -gt 0 ]; then
		avg=$(echo "$sum / $count" | bc -l)
		echo "B+ Tree arity $r, Ray Blocking: Average over $count runs: ${avg}s"
	else
		echo "No results found to average."
	fi
done
