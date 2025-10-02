#!/bin/bash

REALPATH=`dirname $0`
REALPATH=`realpath "${REALPATH}"`
REALHOME=`realpath ~`

#Cleanup pre-exiting tmux sessions
${REALPATH}/../util/cleanup-tmux-session.sh

SERVER="fixpoint-server"

WORKING_DIR="/mnt/fix/compile-server"
CLIENT_WORKING_DIR="/mnt/fix/compile-client"

# Create tmux session, where each pane is a ssh-session to a fix node
${REALPATH}/../util/create-tmux-session.sh ${REALPATH}/compile-server-working-dir/hosts ubuntu

# cd working directory on every server
${REALPATH}/../util/send-to-all-panes.sh "cd ${WORKING_DIR}"

# cd client working directory
cd ${CLIENT_WORKING_DIR}

rm -rf ${REALPATH}/out

for i in {1..6}; do
	${REALPATH}/../util/send-to-all-panes.sh "./${SERVER}.sh"
	sleep 3
	./run.sh >> ${REALPATH}/out
	${REALPATH}/../util/send-to-all-panes.sh "killall fixpoint-server"
	sleep 3
done

#Cleanup the created tmux session
${REALPATH}/../util/cleanup-tmux-session.sh

values=$(grep "Duration" "${REALPATH}/out" | awk '{gsub("s","",$3); print $3}')

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
