#!/usr/bin/env bash
# Usage: ./scatter.sh ip-or-hostname1:path/to/folder ip-or-hostname2:path/to/folder...

if [ "$#" -eq 0 ]
then
  echo "Must specify targets to scatter files." >&2
  exit 1
fi

trap "exit" INT

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
  echo "Sending template fix directory to $target."
  rsync -a repo-template/ $target/.fix/
  echo "Erasing relations not from bootstrap on $target."
  rsync -a --delete repo-template/relations $target/.fix/relations
done

for i in $(seq 0 $(($M-1)))
do
  target=${NODES[$i]}
  echo "Sending $QUOTIENT file(s) to $target."
  for j in $(seq 0 $(($QUOTIENT - 1)))
  do
    k=$(($i * $QUOTIENT + $j))
    rsync -a --ignore-existing .fix/data/${CHUNKS[$k]} $target/.fix/data/ &
  done
  wait
done
for j in $(seq 0 $(($REMAINDER - 1)))
do
  k=$(($M * $QUOTIENT + $j))
  target=${NODES[$j]}
  echo "Sending extra file to $target."
  rsync -a --ignore-existing .fix/data/${CHUNKS[$k]} $target/.fix/data/ &
done
wait


target=${NODES[0]}
echo "Sending label to $target."
rsync -a $(readlink -f .fix/labels/wikipedia) $target/.fix/data/
rsync -a .fix/labels/wikipedia $target/.fix/labels/
