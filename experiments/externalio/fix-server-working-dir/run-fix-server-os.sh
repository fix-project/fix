#!/bin/bash

echo "~/fix/build/src/tester/fixpoint-server 12345 -a $(hostname -i) -p hosts -t 200 -s random -o"
~/fix/build/src/tester/fixpoint-server 12345 -a $(hostname -i) -p hosts -t 200 -s random -o &
