#!/bin/bash


usage() {
    echo "Usage: $0 command"
    echo
    echo "  command : command to send to all panes in the fix-server-session tmux session"
    exit 1
}

if [ $# -ne 1 ] || [[ "$1" == "-h" ]] || [[ "$1" == "--help" ]]; then
    usage
fi

COMMAND=$1
WINDOW_NUMBER="0"
SESSION="fix-server-session"

echo "tmux send-keys -t \"${SESSION}:${WINDOW_NUMBER}\" \"${COMMAND}\" C-m"
tmux send-keys -t "${SESSION}:${WINDOW_NUMBER}" "${COMMAND}" C-m
