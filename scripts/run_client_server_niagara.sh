#!/opt/csw/bin/bash

num_msgs=100000;
reps=10;

num_cores=64;


run_avg=$(gfind -name "run_avg_niagara.sh");

echo "#cores oneway roundtrip"

for num_core in $(seq 2 1 $num_cores)
do
    printf "%-8d" $num_core
    oneway=$($run_avg $reps ./main $num_core $num_msgs $num_cores | gawk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    printf "%-8.0f" $oneway;
    roundtrip=$($run_avg $reps ./main_rt $num_core $num_msgs $num_cores | gawk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    printf "%-6.0f\n" $roundtrip

done;

