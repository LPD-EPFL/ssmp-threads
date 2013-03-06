#!/bin/sh

num_msgs=1000000;
reps=3;

if [ $(uname -n) = "lpd48core" ];
then
    base_core=0;
    other_cores="1 6 12 18";
elif [ $(uname -n) = "diassrv8" ];
then
    base_core=1;
    other_cores="2 11 31"
elif [ $(uname -n) = "smal1.sics.se" ];
then
    num_msgs=1000000;
    base_core=0;
    other_cores="1 35"
elif [ $(uname -n) = "parsasrv1.epfl.ch" ];
then
    num_msgs=1000000;
    base_core=0;
    other_cores="1 35"
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


