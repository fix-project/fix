#!/bin/bash

REALPATH=`dirname $0`
REALPATH=`realpath "${REALPATH}"`
cd ${REALPATH}

./run-compile.sh | tee /tmp/exp_output.txt
values=$(grep "Duration: " /tmp/exp_output.txt | awk '{print $2}')

sum=0
count=0

for v in $values; do
    sum=$(echo "$sum + $v" | bc -l)
    count=$((count + 1))
done

echo /tmp/exp_output.txt

if [ $count -gt 0 ]; then
    avg=$(echo "$sum / $count" | bc -l)
    echo "Average over $count runs: ${avg}ms"
else
    echo "No results found to average."
fi
