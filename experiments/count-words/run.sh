#!/bin/bash

REALPATH=`dirname $0`
REALPATH=`realpath "${REALPATH}"`
REALHOME=`realpath ~`

#Cleanup pre-exiting tmux sessions
${REALPATH}/../util/cleanup-tmux-session.sh


SERVER=(
	"fixpoint-server"
	"fixpoint-server-random"
	"fixpoint-server-random-os"
)

WORKING_DIR="/mnt/fix/count-words"
CLIENT_WORKING_DIR="/mnt/fix/count-words-client"

# Create tmux session, where each pane is a ssh-session to a fix node
${REALPATH}/../util/create-tmux-session.sh ${REALPATH}/count-words-server-working-dir/hosts ubuntu

# cd working directory on every server
${REALPATH}/../util/send-to-all-panes.sh "cd ${WORKING_DIR}"

# cd client working directory
cd ${CLIENT_WORKING_DIR}

for s in "${SERVER[@]}"
do
	rm -rf ${REALPATH}/$s-out
	for i in {1..17}; do
		${REALPATH}/../util/send-to-all-panes.sh "./$s.sh"
		sleep 3 
		./run-count-word.sh 2>>  ${REALPATH}/$s-out
		${REALPATH}/../util/send-to-all-panes.sh "killall fixpoint-server"
		sleep 3
	done

	values=$(grep "Real Time:" "${REALPATH}/${s}-out" | awk '{print $3}')
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
		echo "$s: Average over $count runs (skipping the warm-up pass): ${avg}s"
	else
		echo "No results found to average."
	fi

	sleep 10
done

#Cleanup the created tmux session
${REALPATH}/../util/cleanup-tmux-session.sh
