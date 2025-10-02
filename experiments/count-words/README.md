# Setup
Setup Fix based on README.md on 10 nodes. All of them are server nodes. Pick one of them to also serve as the client node. 

# Fix Server
Change `count-words-server-working-dir/hosts` with content:
```
<server-0-address>:12345
<server-1-address>:12345
<server-2-address>:12345
...
<server-9-address>:12345
```

Copy `count-words-server-working-dir` to every node.

Set up the working directory:
```
cd count-words-server-working-dir
tar -xvzf count-words-server-<index>.tar.gz
```

Start the servers on every node:
```
./fixpoint-server.sh
```
For no-locality ablation:
```
./fixpoint-server-random.sh
```
For no-locality + internal I/O
```
./fixpoint-server-random-os.sh
```

# Fix client
Copy `count-words-client-working-dir` to the client node.

Set up the working directory:
```
cd count-words-client-working-dir
tar -xzvf count-words-client.tar.gz
```

Start the client:
```
./run-count-word.sh
```

# On pre-existing AWS instances
```
./run.sh
```
