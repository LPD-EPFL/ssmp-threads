#! /bin/sh

for command in "./threadclient_server" "./client_server"
do
	echo $command
	for location in 6 12 18 24 30 36 48 
	do
		awk -v e="$command $location" ' BEGIN{sum = 0; count = 0} $0 ~ e {sum += $4; count++} END{print e, (sum/count/1000000)} ' $1 
	done
done

