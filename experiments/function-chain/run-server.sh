#!/bin/bash

REALHOME=`realpath ~`

cd ${REALHOME}/fix
${REALHOME}/fix/build/src/tester/fixpoint-server -s local -t 1 12345
