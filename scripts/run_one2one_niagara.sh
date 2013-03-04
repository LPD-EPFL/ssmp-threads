#!/opt/csw/bin/bash

num_msgs=10000000;
reps=10;

base_core=0;
other_cores="1 8";

run_avg=$(gfind -name "run_avg_niagara.sh");

echo "#c1 c2 oneway roundtrip"

for core in $other_cores
do
    printf "%-4d%-3d" $base_core $core;
    oneway=$($run_avg $reps ./one2one 2 $num_msgs $base_core $core | gawk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    printf "%-8.0f" $oneway;
    roundtrip=$($run_avg $reps ./one2one_rt 2 $num_msgs $base_core $core | gawk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    printf "%-6.0f\n" $roundtrip
done;


