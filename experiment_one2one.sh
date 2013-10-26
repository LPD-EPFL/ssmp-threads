#!/bin/bash

for command in "./threadone2one" "./one2one"
do
	x=0 #x thread on core 0 on die 0
	declare -A location
	location[1]="same-die" #both on die 0
	location[6]="same-mcm" #die 1
	location[24]="one-hop" #die 4
	location[12]="two-hop" #die 2, ie longest path 
	echo "----------"
	echo $command
	for core in ${!location[@]}
	do
		lo=${location[$core]}
		echo $lo
		for i in 1 2 3
		do
			$command -x $x -y $core  > tmp
			if [ $? -eq 0 ] 
			then
				awk -v c=$command -v dist=$lo \
					'BEGIN {ticks=0} 
					/ticks/ {ticks=$18} 
					/Throughput/ {print c, dist, ticks, $4; ticks=-1}'\
					tmp >> measures	
			fi
		done
	done
done

rm tmp
