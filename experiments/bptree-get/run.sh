#!/bin/bash

WORKING_DIR="/mnt/minio/bptree-wikipedia/server"
CLIENT_WORKING_DIR="/mnt/minio/bptree-wikipedia/client"

cd ${CLIENT_WORKING_DIR}
./run-experiments.sh

bptree_roots=(16777216 4096 256 64)
for r in "${bptree_roots[@]}"
do
	# Remove the data for warm-up pass
	rm -rf fix-run-${r}-good-0
	values=$(cat fix-run-"${r}"-* | grep "Duration" | awk '{gsub("s","",$3); print $3}' )

	sum=0
	count=0

	for v in $values; do
		sum=$(echo "$sum + $v" | bc -l)
		count=$((count + 1))
	done

	if [ $count -gt 0 ]; then
		avg=$(echo "$sum / $count" | bc -l)
		echo "B+ Tree arity $r: Average over $count runs: ${avg}s"
	else
		echo "No results found to average."
	fi

done
