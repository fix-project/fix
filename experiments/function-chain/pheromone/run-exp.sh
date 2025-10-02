#!/bin/bash

set -xe

cd ~/pheromone

# Compile the program
docker run --rm -v /home/ubuntu/pheromone/:/users/kevinztw -it ubuntu:18.04 bash -c \
    "apt update && \
    apt install -y g++ && \
    cd /users/kevinztw/examples/cpp/function_chain && \
    chmod +x compile.sh && \
    ./compile.sh"

# Upload .so files to kube pods
pushd examples/cpp/function_chain

func_nodes=$(kubectl get pod | grep func | cut -d " " -f 1 | tr -d " ")
for pod in ${func_nodes[@]}; do
    kubectl cp inc.so $pod:/dev/shm -c local-sched &
done

wait

popd

# Run the experiment
cd client/pheromone
python3 -m benchmarks.func_chain
