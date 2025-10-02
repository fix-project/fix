#!/bin/bash

cd ~/pheromone
export PHERO_HOME=$(pwd)                                                                                                                            

# Make pheromone client ports accessible to the non-Kubernete stuffs
kubectl apply -f fixpoint-main.yml
kubectl apply -f snr-endpoint.yml

kubectl label nodes ip-172-31-12-100 role=function
kubectl label nodes ip-172-31-25-125 role=general
kubectl label nodes ip-172-31-32-160 role=routing
kubectl label nodes ip-172-31-32-58 role=memory
kubectl label nodes ip-172-31-40-42 role-
kubectl label nodes ip-172-31-44-169 role=coordinator
kubectl label nodes ip-172-31-46-145 role-
kubectl label nodes ip-172-31-9-0 role-
kubectl label nodes ip-172-31-9-144 role-

# Set the function setup to the original setup
pushd deploy/cloudlab/deploy/cluster/yaml/ds/
cp function-ds-original.yml function-ds.yml 
popd

# Create the cluster
pushd deploy/cloudlab
python3 -m deploy.cluster.create_cluster  -m 1 -r 1 -c 1 -f 1
popd

# Get management-service external port and modify pheromone to use that
MANAGEMENT_PORT=`kubectl get svc management-service -o jsonpath={.spec.ports[0].nodePort}`
sed -i "s/^COORD_QUERY_PORT = [0-9]\+/COORD_QUERY_PORT = ${MANAGEMENT_PORT}/" client/pheromone/utils.py
