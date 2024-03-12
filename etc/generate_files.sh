#!/bin/bash

# Generate files
SCRIPTS="applications/flatware/generate.py"

for SCRIPT in $SCRIPTS; do
    echo "Running $SCRIPT"
    python $SCRIPT
done
```
