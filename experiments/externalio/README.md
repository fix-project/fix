# Setup
Setup Fix based on README.md on three nodes, one for the client, one for the server, one for the remote data server. Make sure that the source directory is at `~/fix`.


# Data Server
Copy `dataserver-working-dir` to the data server node.

Set up the working directory:
```
cd dataserver-working-dir
tar -xzvf data-server.tar.gz
```

Start the data server:
```
./run-dataserver.sh
```

# Fix Server
Copy `fix-server-working-dir` to the fix server node

Set up the working directory:
```
cd fix-server-working-dir
tar -xzvf bootstrap.tar.gz
```

Set up `hosts` with content:
```
<data-server-address>:12345
<fix-server-address>:12345
```

Start the Fix server:
```
./run-fix-server.sh
```
For Fix with internal I/O, instead:
```
./run-fix-server-os.sh
```

# Fix Client
Copy `client-working-dir` to the client node

Set up the working directory:
```
cd client-working-dir 
tar -xzvf oneoff-client.tar.gz
```

Start the client:
```
./run.sh <server-address>
```
