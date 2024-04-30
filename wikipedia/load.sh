#!/usr/bin/env bash

fix init
refs=""
for chunk in /usr/local/share/wikipedia/chunks/*
do
  echo $chunk
  refs+=$(fix ref $(fix add $chunk))
  refs+=" "
done
fix create-tree -l "wikipedia" $refs
