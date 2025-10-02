#!/bin/bash

if [ "$#" -lt 1 ]; then
  echo "Error: This script requires server address"
  echo "Usage: $0 <server-address>"
  exit 1
fi

REALHOME=`realpath ~`

cd "${REALHOME}/fix"

echo "${RELHOME}/fix/build/src/tester/fixpoint-client-dec 500 ${1}:12345"
${REALHOME}/fix/build/src/tester/fixpoint-client-dec 500 ${1}:12345
