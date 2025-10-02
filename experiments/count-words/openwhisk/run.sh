#!/bin/bash

REALPATH=`dirname $0`
REALPATH=`realpath "${REALPATH}"`
cd ${REALPATH}

./run-count-words.sh | tee /tmp/exp_output.txt
values=$(grep "Duration: " /tmp/exp_output.txt | awk '{gsub("ms","",$2); print $2}')

sum=0
count=0
skip_first=1

for v in $values; do
    if [ $skip_first -eq 1 ]; then
        skip_first=0
        continue
    fi
    sum=$(echo "$sum + $v" | bc -l)
    count=$((count + 1))
done

echo /tmp/exp_output.txt

if [ $count -gt 0 ]; then
    avg=$(echo "$sum / $count" | bc -l)
    echo "Average over $count runs (skipping the warm-up pass): ${avg}ms"
else
    echo "No results found to average."
fi
