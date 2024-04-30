#!/usr/bin/env bash
# Usage: ./scatter.sh ip-or-hostname1:path/to/.fix ip-or-hostname2:path/to/.fix...

if [ "$#" -eq 0 ]
then
  echo "Must specify targets to scatter files." >&2
  exit 1
fi

NODES=($@)
IFS=$'\n'
CHUNKS=($(fix ls-tree wikipedia | cut -d' ' -f4 | sed "s/500\$/400/"))
M=${#NODES[@]}
N=${#CHUNKS[@]}
QUOTIENT=$((N / M))
REMAINDER=$((N % M))

for i in $(seq 0 $(($M-1)))
do
  target=${NODES[$i]}
  for j in $(seq 0 $(($QUOTIENT - 1)))
  do
    k=$(($i * $QUOTIENT + $j))
    rsync -a --ignore-existing .fix/data/${CHUNKS[$k]} $target
  done
done
for j in $(seq 0 $(($REMAINDER - 1)))
do
  k=$(($M * $QUOTIENT + $j))
  target=${NODES[$j]}
  rsync -a --ignore-existing .fix/data/${CHUNKS[$k]} $target
done
