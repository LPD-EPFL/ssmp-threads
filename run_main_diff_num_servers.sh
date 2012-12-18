#!/bin/bash

NUM_MSG=500000
DELAY_AFTER=400

for server in $@
do
    echo "" > ssmp.servers.$server;
    echo "Servers: 1 per $server cores -------------------------------------------------------"
    for i in $(seq 2 1 48)
    do
	oneway=$(./main_oneway $i $NUM_MSG $server | awk '/Total/ { print $4 }');
	rountrip=$(./main_rountrip $i $NUM_MSG $server $DELAY_AFTER | awk '/Total/ { print $4 }');
	printf "%-3d%12.1f%12.1f\n" $i $oneway $rountrip | tee -a ssmp.servers.$server.delay
    done
done

