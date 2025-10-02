#!/bin/bash

WINDOW_NUMBER="0"
SESSION="fix-server-session"

# Start a new tmux session
tmux kill-session -t ${SESSION}
tmux new-session -d -s ${SESSION} 
tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%0" "ssh 172.31.40.42" C-m
tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%0" "ray start --head --num-cpus 1" C-m

sleep 5

REALPATH=`dirname $0`
REALPATH=`realpath "${REALPATH}"`

rm -rf ${REALPATH}/out

cd ~/ray-benchmark/local

for ((i=0; i<5; i++))
do
	python3 function-chain.py 500 172.31.40.42 >> ${REALPATH}/out 
done

values=$(cat "${REALPATH}"/out | awk '{print $1}')

sum=0
count=0

for v in $values; do
    sum=$(echo "$sum + $v" | bc -l)
    count=$((count + 1))
done

if [ $count -gt 0 ]; then
    avg=$(echo "$sum / $count" | bc -l)
    echo "Average over $count runs: ${avg}s"
else
    echo "No results found to average."
fi

tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%0" "ray stop" C-m
tmux kill-session -t ${SESSION}
