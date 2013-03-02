#!/bin/sh

num_msgs=10000000;
reps=10;

if [ $(uname -n) = "lpd48core" ];
then
    base_core=0;
    other_cores="1 6 12 18";
else
    base_core=1;
    other_cores="10 40 80"
fi

run_avg=$(find -name "run_avg.sh");

echo "#c1 c2 oneway roundtrip"

for core in $other_cores
do
    printf "%-4d%-3d" $base_core $core;
    oneway=$($run_avg $reps ./one2one 2 $num_msgs $base_core $core | awk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    printf "%-8.0f" $oneway;
    roundtrip=$($run_avg $reps ./one2one_rt 2 $num_msgs $base_core $core | awk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    printf "%-6.0f\n" $roundtrip

done;


