#!/bin/bash

# set local address
LOCAL=`hostname -i`
~/fix/build/src/tester/fixpoint-server -a ${LOCAL} -p /home/ubuntu/hosts 12345 &
