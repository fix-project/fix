* Setup Fixpoint based on README.md on two nodes, one for the client, one for the server

* Start the server (from the source directory of `Fixpoint`)
```
./experiments/function-chain/run-server.sh
```

* Start the client (from the source directory of `Fixpoint`)
```
./experiments/function-chain/run-client.sh <server-address>
```
e.g. if the server and the client are on the same machine
```
./experiments/function-chain/run-client.sh 127.0.0.1
```

# On pre-existing AWS instances
```
./run.sh

