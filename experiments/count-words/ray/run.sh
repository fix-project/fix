#!/bin/bash

REALPATH=`dirname $0`
REALPATH=`realpath "${REALPATH}"`
REALHOME=`realpath ~`

SESSION="fix-server-session"
WINDOW_NUMBER=0

#Cleanup pre-exiting tmux sessions
${REALPATH}/../../util/cleanup-tmux-session.sh

# Create tmux session, where each pane is a ssh-session to a fix node
${REALPATH}/../../util/create-tmux-session.sh ${REALPATH}/../count-words-server-working-dir/hosts ubuntu

# Set file descriptor limit
tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}" "ulimit -n 65536" C-m

# Disable pane synchronization since the command is different for each panes
tmux set-window-option -t ${SESSION}:${WINDOW_NUMBER} synchronize-panes off

# Start ray
tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%0" "ray start --head" C-m
sleep 5
for ((i=1; i<=9;i++)); do
	tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%${i}" "ray start --address='172.31.8.132:6379'" C-m
done

sleep 5

cd ~/ray-benchmark/cluster
rm -rf ${REALPATH}/raycp-out
rm -rf ${REALPATH}/raybl-out

for i in {1..17}; do
	python3 count-words.py good /mnt/fix/count-words/.fix chunk-100M A27 >> ${REALPATH}/raycp-out
	sleep 5
done

for i in {1..17}; do
	python3 count-words.py bad /mnt/fix/count-words/.fix chunk-100M A27 >> ${REALPATH}/raybl-out
	sleep 5
done

raycp_values=$(cat "${REALPATH}"/raycp-out | grep "Duration:" | awk '{print $2}' )
raybl_values=$(cat "${REALPATH}"/raybl-out | grep "Duration:" | awk '{print $2}' )

sum=0
count=0
skip_first=1

for v in $raycp_values; do
	if [ $skip_first -eq 1 ]; then
		skip_first=0
		continue
	fi
	sum=$(echo "$sum + $v" | bc -l)
	count=$((count + 1))
done

if [ $count -gt 0 ]; then
	avg=$(echo "$sum / $count" | bc -l)
	echo "Ray Continuation Passing: Average over $count runs: ${avg}s"
else
	echo "No results found to average."
fi

sum=0
count=0
skip_first=1

for v in $raybl_values; do
	if [ $skip_first -eq 1 ]; then
		skip_first=0
		continue
	fi
	sum=$(echo "$sum + $v" | bc -l)
	count=$((count + 1))
done

if [ $count -gt 0 ]; then
	avg=$(echo "$sum / $count" | bc -l)
	echo "Ray Blocking: Average over $count runs: ${avg}s"
else
	echo "No results found to average."
fi

sleep 10

for ((i=0; i<=9;i++)); do
	tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%${i}" "ray stop" C-m
done

sleep 10

for ((i=0; i<=9;i++)); do
	tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%${i}" "ray stop" C-m
done

#Cleanup the created tmux session
${REALPATH}/../../util/cleanup-tmux-session.sh
