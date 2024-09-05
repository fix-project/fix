#!/usr/bin/env bash
# Usage: ./scatter.sh path/to/folder path/to/folder...

if [ "$#" -eq 0 ]
then
  echo "Must specify targets to scatter files." >&2
  exit 1
fi

trap "exit" INT

NODES=($@)
IFS=$'\n'
M=${#NODES[@]}

for i in $(seq 0 $(($M-1)))
do
  target=${NODES[$i]}
  echo "Sending template fix directory to $target."
  rsync -a --delete repo-template/ $target/.fix/ &
done
wait

for filename in .fix/data/*
do
  target=${NODES[$(($RANDOM%M))]}
  cp $filename $target/.fix/data/ &
done
wait

for i in $(seq 0 $(($M-1)))
do
  target=${NODES[$i]}
  echo "Sending template fix directory to $target."
  cp -a ./.fix/labels/tree-root $target/.fix/labels/tree-root &
done
wait
