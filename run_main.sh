#!/bin/bash

for i in $(seq 2 1 48)
do
    oneway=$(./main_oneway $i $@ | awk '/Total/ { print $4 }');
    rountrip=$(./main_rountrip $i $@ | awk '/Total/ { print $4 }');
    printf "%-3d%12.1f%12.1f\n" $i $oneway $rountrip
done

