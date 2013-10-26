#!/bin/bash

for command in "./threadclient_server" "./client_server"
do
	echo "----------"
	echo $command
	for core in 6 12 18 24 30 36 48
	do
			$command -n $core -s $core  > tmp
			if [ $? -eq 0 ] 
			then
				awk -v c=$command -v co=$core\
					'BEGIN {ticks=0} 
					/ticks/ {ticks=$18} 
					/throughput/ {print c,co, ticks, $4; ticks=-1}'\
					tmp >> measures 
			fi
	done
done

rm tmp
