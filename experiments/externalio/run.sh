#!/bin/bash

REALPATH=`dirname $0`
REALPATH=`realpath "${REALPATH}"`
REALHOME=`realpath ~`

#Cleanup pre-exiting tmux sessions
${REALPATH}/../util/cleanup-tmux-session.sh

SERVER=(
	"run-fix-server"
	"run-fix-server-os"
)

WORKING_DIR="${REALHOME}/oneoff-server"
DATASERVER_WORKING_DIR="${REALHOME}/oneoff"
CLIENT_WORKING_DIR="${REALHOME}/oneoff"

# Create tmux session, where each pane is a ssh-session to a fix node
${REALPATH}/../util/create-tmux-session.sh ${REALPATH}/fix-server-working-dir/hosts ubuntu

# Disable pane synchronization since the command is different for each panes
tmux set-window-option -t ${SESSION}:${WINDOW_NUMBER} synchronize-panes off

# cd working directory of data server on node 0
tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%0" "cd ${DATASERVER_WORKING_DIR}" C-m

# cd working directory of fix server on node 1
tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%1" "cd ${WORKING_DIR}" C-m

# cd client working directory
cd ${CLIENT_WORKING_DIR}

for s in "${SERVER[@]}"
do
	for i in {1..6}; do
		# Start data server
		tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%0" "./run-dataserver.sh" C-m
		sleep 1

		# Start fix server
		tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%1" "./${s}.sh" C-m
		sleep 1

		# Start client
		./run.sh 2>> ${REALPATH}/$s-out

		# Kill fix server
		tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%1" "killall fixpoint-server " C-m

		# Kill data server
		tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%0" "killall data-server " C-m
		sleep 1
	done
done

#Cleanup the created tmux session
${REALPATH}/../util/cleanup-tmux-session.sh
