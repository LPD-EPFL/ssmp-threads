#!/bin/bash

do_throughput=0;

starting_num_cores=2;

if [ $# -gt 0 ];
then
    starting_num_cores=$1;
fi

num_msgs=100000;
reps=1;

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
    num_msgs=1000;
    reps=1;
elif [ $(uname -n) = "maglite" ];
then
    num_cores=64;
fi


# dsl_per_core=$num_cores;
dsl_per_core=3;

run_avg=$(find . -name "run_avg.sh");

echo "#cores oneway           roundtrip"

for num_core in $(seq $starting_num_cores 1 $num_cores)
do
    printf "%-8d" $num_core

#     if [ $do_throughput -eq 1 ];
#     then
# 	oneway=$($run_avg $reps ./main $num_core $num_msgs $dsl_per_core $delay_after | gawk '/through/ { sum+=$4; r++ } END {print sum/r}');
#     else
# 	oneway=$($run_avg $reps ./main $num_core $num_msgs $dsl_per_core $delay_after | gawk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');

#     fi
    printf "%-16.0f" $oneway;
    if [ $do_throughput -eq 1 ];
    then
	roundtrip=$($run_avg $reps ./main_rt $num_core $num_msgs $dsl_per_core | gawk '/through/ { sum+=$4; r++ } END {print sum/r}');
    else
	roundtrip=$($run_avg $reps ./main_rt $num_core $num_msgs $dsl_per_core | gawk '/ticks/ { sum+=$18; r++ }; END {print sum/r}');
    fi

    printf "%-16.0f\n" $roundtrip;

done;

