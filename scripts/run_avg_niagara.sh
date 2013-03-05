#!/opt/csw/bin/bash

reps=$1;
shift;
execute=$@;

for rep in $(seq 1 1 $reps)
do
    ./$execute
done;


