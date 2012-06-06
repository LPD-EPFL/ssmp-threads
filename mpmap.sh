#!/bin/sh

CORES=$(cat /proc/cpuinfo | grep processor | tail -n1 | awk '// {print $3}');
echo "Found $(($CORES+1)) cores"

echo -n "     ";

for ccol in $(seq 0 1 $CORES); do
    printf "%5d" $ccol
done

echo "";
echo -n "------"

for ccol in $(seq 0 1 $((5*$(($CORES+1))))); do
    echo -n "-"
done

echo "";

for crow in $(seq 0 1 $CORES); do
    printf "%5d |" $crow
    for ccol in $(seq 0 1 $CORES); do
	if [ "$crow" -ne "$ccol" ]
	then
	    res=$(./$1 2 $2 $crow $ccol | grep "ticks/msg" | awk '// {sum+=$1}; END {printf "%d", sum/NR}')
	    printf "%5d" $res
	else
	    echo -n "    0"
	fi
    done
    echo "";
done


