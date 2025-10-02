#!/bin/bash

cd ~/owdeploy

helm uninstall owdev -n openwhisk
helm install owdev openwhisk-deploy-kube/helm/openwhisk -n openwhisk --create-namespace -f mycluster.yaml

while true; do
	if kubectl get pods -n openwhisk | grep "install-package" | grep Completed -o > /dev/null; then
		break
	fi
	sleep 10
done

wsk action create lwvirt actions/lwvirt/lwvirt.zip --kind nodejs:14 -i --timeout 1200000 --memory 2048

for ((i=0; i<6; i++))
do
	result=$(wsk action invoke lwvirt -i)
	activation=$(echo "$result" | awk '{print $NF}')

	while true; do
		sleep 20
		record=$(wsk activation -i get $activation)
		if [ -z "$record" ]; then
			continue	
		else
			record=$(echo "$record" | tail -n +2)
			duration=$(echo "$record" | grep stdout | awk '{gsub("\"","",$4); print $4}')
			echo "Duration: ${duration}ms"
			break
		fi
	done
done

wsk action delete -i lwvirt
helm uninstall owdev -n openwhisk
kubectl delete --all pods --namespace=openwhisk --grace-period=0 --force
