#!/bin/bash

cd ~/owdeploy

for ((j=0; j<5; j++))
do
	helm uninstall owdev -n openwhisk
	pushd ~/ansible
	./clean-compile-images.sh > /dev/null
	popd
	
	helm install owdev openwhisk-deploy-kube/helm/openwhisk -n openwhisk --create-namespace -f mycluster.yaml
	
	while true; do
		if kubectl get pods -n openwhisk | grep "install-package" | grep Completed -o > /dev/null; then
			break
		fi
		sleep 10
	done
	
	get_time() {
	    date +%s%3N
	}
	
	start_time=$(get_time)
	echo $start_time
	/mnt/fix/compile-client/upload-file.sh
	
	wsk action create compilepoll actions/compilepoll/compilepoll.zip --kind nodejs:14 -i --timeout 1200000
	result=$(wsk action invoke compilepoll -i --param input_bucket wasm-files --param input_file link-elfs-fix-wasm --param output_bucket lld-out --param minio_url 10.103.170.222:80 --param output_number 1977)
	activation=$(echo "$result" | awk '{print $NF}')
	
	sleep 90
	
	while true; do
		sleep 1
		record=$(wsk activation -i get $activation)
		if [ -z "$record" ]; then
			continue	
		else
			end_time=$(wsk activation -i get $activation | grep end | head -n 1 | grep '[0-9]*' -o)
			break
		fi
	done
	
	duration=$((end_time - start_time))
	echo "Duration: $duration milliseconds"
done

helm uninstall owdev -n openwhisk
kubectl delete --all pods --namespace=openwhisk --grace-period=0 --force
