#!/usr/bin/env bash

SRC_REL=`dirname $0`
SRC=`realpath ${SRC_REL}`

WINDOW_NUMBER="0"
SESSION="fix-server-session"

# Start a new tmux session
tmux new-session -d -s ${SESSION} 
tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%0" "ssh 172.31.40.42" C-m
tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%0" "cd ~/fix" C-m

cd ~/fix

for ((j=0; j<5; j++))
do
	tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%0" "${SRC}/run-server.sh &" C-m
	sleep 5
	${SRC}/run-client.sh 172.31.40.42
	tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%0" "killall fixpoint-server" C-m
	sleep 1
done

# Cleanup tmux session
${SRC}/../util/cleanup-tmux-session.sh
