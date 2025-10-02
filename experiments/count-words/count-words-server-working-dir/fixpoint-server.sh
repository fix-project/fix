#!/bin/sh -xe

# set local address
LOCAL=`hostname -i`
SERVER=~/fix/build/src/tester/fixpoint-server

${SERVER} -a ${LOCAL} -p hosts 12345 &
