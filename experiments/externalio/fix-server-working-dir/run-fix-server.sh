#!/bin/bash

echo "~/fix/build/src/tester/fixpoint-server 12345 -a $(hostname -i) -p hosts"
~/fix/build/src/tester/fixpoint-server 12345 -a $(hostname -i) -p hosts &
