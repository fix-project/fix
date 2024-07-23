#!/usr/bin/env bash

set +e
if [ -z "$1" ]
then
  if src/tests/test-resource-limits 2>/dev/null
  then
    echo "Error: execution did not trap." >&2
    exit 1
  fi
else
  if src/tests/test-resource-limits -s $1 2>/dev/null
  then
    echo "Error: execution did not trap." >&2
    exit 1
  fi
fi
