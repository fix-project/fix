#!/bin/bash

if [ "$#" -lt 1 ]; then
  echo "Error: This script requires server address"
  echo "Usage: $0 <server-address>"
  exit 1
fi

echo "~/fix/build/src/tester/fixpoint-client-oneoff input1024 ${1}:12345"
~/fix/build/src/tester/fixpoint-client-oneoff input1024 ${1}:12345
