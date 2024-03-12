#!/usr/bin/env bash

set +e
if src/tests/test-resource-limits 2>/dev/null
then
  echo "Error: execution did not trap." >&2
  exit 1
fi
