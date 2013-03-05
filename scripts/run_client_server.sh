#!/bin/sh

starting_num_cores=2;

if [ $# -gt 0 ];
then
    starting_num_cores=$1;
fi

num_msgs=100000;
reps=5;

if [ $(uname -n) = "lpd48core" ];
then
    base_core=0;
    num_cores=48;
else
    base_core=1;
    other_cores="10 40 80"
fi

run_avg=$(find -name "run_avg.sh");

echo "#cores oneway roundtrip"

for num_core in $(seq $starting_num_cores 1 $num_cores)
do
    printf "%-8d" $num_core
    oneway=$($run_avg $reps ./main $num_core $num_msgs $num_cores | awk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    printf "%-8.0f" $oneway;
    roundtrip=$($run_avg $reps ./main_rt $num_core $num_msgs $num_cores | awk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    printf "%-6.0f\n" $roundtrip

done;


