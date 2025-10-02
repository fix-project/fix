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

experiments=(
	"chunk-100m 0 984"
)

i=0
for exp in "${experiments[@]}"
do
	curr_param=($exp)
	for ((j=0; j<17; j++))
	do
		wsk action create mapreduce-$i actions/mapreduce/mapreduce.zip --kind nodejs:14 -i --timeout 1200000 --memory 2048

		echo "wsk action invoke mapreduce-$i -i --param input_bucket ${curr_param[0]} --param start ${curr_param[1]} --param end ${curr_param[2]} --param index $i"
		result=$(wsk action invoke mapreduce-$i -i --param input_bucket ${curr_param[0]} --param start ${curr_param[1]} --param end ${curr_param[2]} --param index $i)
		activation=$(echo "$result" | awk '{print $NF}')

		while true; do
			sleep 10
			record=$(wsk activation -i get $activation)
			if [ -z "$record" ]; then
			        continue	
			else
				record=$(echo "$record" | tail -n +2)
				duration=$(echo "$record" | grep "duration" | awk '{gsub(",","",$2); print $2}')
				echo "Duration: ${duration}ms"
				break
			fi
		done

		wsk action delete -i count-words-$i
		wsk action delete -i merge-counts-$i
		wsk action delete -i mapreduce-$i
		i=$((i+1))
	done
done

helm uninstall owdev -n openwhisk
kubectl delete --all pods --namespace=openwhisk --grace-period=0 --force
