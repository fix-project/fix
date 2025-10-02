#!/bin/bash

REALPATH=`dirname $0`
REALPATH=`realpath "${REALPATH}"`
cd ${REALPATH}

rm -rf ${REALPATH}/out

cd ~/ray-benchmark/local

for ((i=0; i<5; i++))
do
	python3 add.py >> ${REALPATH}/out 
done

values=$(cat "${REALPATH}"/out | awk '{print $3}')

sum=0
count=0

for v in $values; do
    sum=$(echo "$sum + $v" | bc -l)
    count=$((count + 1))
done

if [ $count -gt 0 ]; then
    avg=$(echo "$sum / $count / 4096" | bc -l)
    echo "Average over $count runs (4096 invocations each): ${avg}ms"
else
    echo "No results found to average."
fi
