# Setup
Setup Fixpoint based on README.md on one node.

Copy `bptree-get/` to the node.

```
cd bptree-get
pushd client
tar -xvzf bptree-client.tar.gz
popd
pushd server
tar -xvzf bptree-server.tar.gz
popd
```

# Run experiment
```
cd client
./run-experiments.sh
```

# On pre-existing AWS instances
```
./run.sh
```

The output is at `/mnt/minio/bptree-wikipedia/client/`
