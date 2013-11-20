#!/bin/bash

for command in "./client_server" 
do
	echo "----------"
	echo $command
	for core in 6 #12 18 24 30 36 48
	do
		for dsl in $((core/1)) $((core/2)) $((core/3)) #$((core/4)) $((core/5)) $((core/6))
		do
			echo $core $dsl
			$command -n $core -s $dsl  > tmp
			if [ $? -eq 0 ] 
			then
				awk -v c=$command -v co=$core -v s=$dsl\
					'BEGIN {ticks=0} 
					/ticks/ {ticks=$18} 
					/throughput/ {print c,co, s, ticks, $4; ticks=-1}'\
					tmp >> measures 
			fi
		done
	done
done

rm tmp
