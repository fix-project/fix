tmux send-keys -t "fix-server-session:0" "killall fixpoint-server" C-m
tmux kill-session -t fix-server-session
