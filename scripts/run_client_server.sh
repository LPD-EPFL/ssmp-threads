#!/bin/sh

starting_num_cores=2;

if [ $# -gt 0 ];
then
    starting_num_cores=$1;
fi

num_msgs=500000;
reps=2;

if [ $(uname -n) = "lpd48core" ];
then
    num_cores=48;
elif [ $(uname -n) = "diassrv8" ];
then
    num_cores=80;
elif [ $(uname -n) = "smal1.sics.se" ];
then
    num_cores=36;
elif [ $(uname -n) = "parsasrv1.epfl.ch" ];
then
    num_cores=36;
fi

run_avg=$(find -name "run_avg.sh");

echo "#cores oneway roundtrip"

for num_core in $(seq $starting_num_cores 1 $num_cores)
do
    printf "%-8d" $num_core
    oneway=$($run_avg $reps ./main $num_core $num_msgs $num_cores $delay_after | awk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    printf "%-8.0f" $oneway;
    roundtrip=$($run_avg $reps ./main_rt $num_core $num_msgs $num_cores | awk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    printf "%-6.0f\n" $roundtrip

done;

