#!/bin/sh

reps=$1;
shift;

run=./run
execute=$@;

for rep in $(seq 1 1 $reps)
do
    $run ./$execute
done;


