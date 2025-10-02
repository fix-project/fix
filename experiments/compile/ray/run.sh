#!/bin/bash

REALPATH=`dirname $0`
REALPATH=`realpath "${REALPATH}"`
REALHOME=`realpath ~`

WINDOW_NUMBER="0"
SESSION="fix-server-session"

#Cleanup pre-exiting tmux sessions
${REALPATH}/../../util/cleanup-tmux-session.sh

# Create tmux session, where each pane is a ssh-session to a fix node
${REALPATH}/../../util/create-tmux-session.sh ${REALPATH}/../compile-server-working-dir/hosts ubuntu

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
rm -rf ${REALPATH}/out

for i in {1..6}; do
	python3 compile-subprocess.py /mnt/minio/ow-actions/build/src/driver/ wasm-files link-elfs-fix-wasm lld-out 32338 1977 -d >> ${REALPATH}/out
	sleep 5
done

values=$(cat "${REALPATH}"/out | grep "Duration:" | awk '{print $2}' )

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

if [ $count -gt 0 ]; then
	avg=$(echo "$sum / $count" | bc -l)
	echo "Average over $count runs: ${avg}s"
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
