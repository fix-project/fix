#!/bin/bash

REALPATH=`dirname $0`
REALPATH=`realpath "${REALPATH}"`
cd ${REALPATH}

./deploy.sh

echo "Waiting for 150 seconds for pheromone to complete setup"
sleep 150
echo "Done waiting"

./run-exp.sh | tee /tmp/exp_output.txt
values=$(grep "Client timer." /tmp/exp_output.txt | awk '{gsub("s","",$3); print $3}')

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
    echo "Average over $count runs (skipping the warm-up pass): ${avg}s"
else
    echo "No results found to average."
fi

./cleanup.sh
