#!/usr/bin/env bash

if [ "$#" -ne 7 ]; then
  echo "Usage: $0 <path-to-fixpoint-build> <path-to-bptree-server-fix> <path-to-bptree-client-fix> <key-list> <num-keys> <bptree-get-label> <tree-root-label>"
  exit 1
fi

SRC_REL=`dirname $0`
SRC=`realpath ${SRC_REL}`
SERVER_BPTREE_PATH=${SRC}/$2
CLIENT_BPTREE_PATH=${SRC}/$3
DRIVER_PATH=$1
KEY_LIST=${SRC}/$4

for ((j=0; j<5; j++))
do
	pushd $SERVER_BPTREE_PATH
	echo "${DRIVER_PATH}/src/tester/fixpoint-server 12345 -s local -t 1 &"
	${DRIVER_PATH}/src/tester/fixpoint-server 12345 -s local -t 1 &
	JOB_PID=$!
	popd

	sleep 5

	begin_index=$((j * $5))
	pushd $CLIENT_BPTREE_PATH
	echo "${DRIVER_PATH}/src/tester/fixpoint-client-bptree +127.0.0.1:12345 $KEY_LIST $begin_index $5 $6 $7"
	${DRIVER_PATH}/src/tester/fixpoint-client-bptree +127.0.0.1:12345 $KEY_LIST $begin_index $5 $6 $7
	popd

	kill $JOB_PID

	sleep 1
done
