#!/bin/sh

num_msgs=1000000;
reps=10;

if [ $(uname -n) = "lpd48core" ];
then
    num_cores=48;
elif [ $(uname -n) = "diassrv8" ];
    num_cores=80;
fi

run_avg=$(find -name "run_avg.sh");

echo "#cores oneway roundtrip"

for num_core in $(seq 2 1 $num_cores)
do
    printf "%-8d" $num_core
    oneway=$($run_avg $reps ./main $num_core $num_msgs $num_cores | awk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    printf "%-8.0f" $oneway;
    roundtrip=$($run_avg $reps ./main_rt $num_core $num_msgs $num_cores | awk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    printf "%-6.0f\n" $roundtrip

done;


