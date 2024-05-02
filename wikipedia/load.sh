#!/usr/bin/env bash

rm -rf .fix
fix init
refs=""
for chunk in /usr/local/share/wikipedia/chunks-1G/*
do
  echo $chunk
  refs+=$(fix ref $(fix add $chunk))
  refs+=" "
done
fix create-tree -l "wikipedia" $refs
