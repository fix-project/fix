#!/bin/bash

usage() {
    echo "Usage: $0 hosts username"
    echo
    echo "  hosts : File containing lines of 'ip:port' (only IP is used)"
    echo "  username    : SSH username"
    exit 1
}

if [ $# -ne 2 ] || [[ "$1" == "-h" ]] || [[ "$1" == "--help" ]]; then
    usage
fi

IP_FILE=$1
USER=$2
WINDOW_NUMBER="0"
SESSION="fix-server-session"

# Start a new tmux session
tmux new-session -d -s ${SESSION} 

# Get the number of hosts
HOST_NUM=`cat ${IP_FILE} | wc -l`
PANE_NUM=`tmux list-panes -t ${SESSION}:0 | wc -l`

if (( ${PANE_NUM} > ${HOST_NUM} )); then
	echo "More existing panes than hosts"
	exit 1
fi

# Create the same number of panes as the number of hosts 
for ((i=PANE_NUM; i<HOST_NUM;i++ )); do
        tmux split-window -t ${SESSION} -h
        tmux select-layout -t ${SESSION} tiled
done

tmux set-window-option -t ${SESSION}:${WINDOW_NUMBER} synchronize-panes off

PANE=0
while read -r line; do
    if [ -z "$line" ]; then
        continue
    fi

    ip=$(echo "$line" | cut -d: -f1)

    echo "tmux send-keys -t \"${SESSION}:${WINDOW_NUMBER}.%${PANE}\" \"ssh ${USER}@${ip}\" C-m"
    tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}.%${PANE}" "ssh ${USER}@${ip}" C-m
    ((PANE++))
done < "$IP_FILE"

tmux set-window-option -t ${SESSION}:${WINDOW_NUMBER} synchronize-panes on
