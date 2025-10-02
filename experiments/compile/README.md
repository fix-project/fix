# Setup
Setup Fix based on README.md on 10 nodes. All of them are server nodes. Pick one of them to also serve as the client node. 

# Fix Server
Change `compile-server-working-dir/hosts` with content:
```
<server-0-address>:12345
<server-1-address>:12345
<server-2-address>:12345
...
<server-9-address>:12345
```

Copy `compile-server-working-dir` to every node.

Set up the working directory:
```
cd compile-server-working-dir
tar -xvzf compile-server.tar.gz
```

Start the servers on every node:
```
./run-server.sh
```

# Fix client
Copy `compile-client-working-dir` to the client node.

Set up the working directory:
```
cd compile-client-working-dir
tar -xzvf compile-client.tar.gz
```

Start the client:
```
./run.sh
```

# On pre-existing AWS instances
```
./run.sh
```
