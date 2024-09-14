#!/usr/bin/env bash
# Usage:./run.sh path-to-fixpoint-build path-to-bptree-fix key-list num-keys max-n

if [ "$#" -ne 5 ]; then
  echo "Usage: $0 <path-to-fixpoint-build> <path-to-bptree-fix> <key-list> <num-keys> <max-n>"
  exit 1
fi

SRC_REL=`dirname $0`
SRC=`realpath ${SRC_REL}`
BPTREE_PATH=${SRC}/$2
DRIVER_PATH=${SRC}/$1
KEY_LIST=${SRC}/$3

pushd $BPTREE_PATH
for ((i=1; i <=$5; i=i+100))
do
  for ((j=0; j<5; j++))
  do
    ${DRIVER_PATH}/src/tester/fixpoint-server 12345 -t 1 &
    JOB_PID=$!

    sleep 5
    begin_index=$((j * $4))
    echo "${DRIVER_PATH}/src/tester/fixpoint-client-bptree-n +127.0.0.1:12345 $KEY_LIST $begin_index $4 $i"
    ${DRIVER_PATH}/src/tester/fixpoint-client-bptree-n +127.0.0.1:12345 $KEY_LIST $begin_index $4 $i

    kill $JOB_PID
  done
done
popd

