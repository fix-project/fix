#!/bin/bash

# origin/master isn't fetched by default on GitHub runners
git fetch --all

FORMAT_MSG=$(git clang-format origin/master -q --diff -- src/)
if [ -n "$FORMAT_MSG" -a "$FORMAT_MSG" != "no modified files to format" ]
then
  echo "Please run git clang-format before committing, or apply this diff:"
  echo
  # Run git clang-format again, this time without capturing stdout.  This way
  # clang-format format the message nicely and add color.
  git clang-format origin/master -q --diff -- src/
  exit 1
fi
