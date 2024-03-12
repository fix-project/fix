#!/bin/bash

# Generate files
SCRIPTS=("applications/flatware/generate.py applications/flatware" )

for SCRIPT in "${SCRIPTS[@]}"; do
    echo "Running $SCRIPT"
    python3 $SCRIPT
done
