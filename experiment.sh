#! /bin/sh

for command in "./threadone2one" "./one2one"
do
	echo $command
	for pair in 2 4 8 16 32 48 64 96
	do
		for i in 1 2 3
		do
			echo "----------"
			echo $pair
			$command $pair > tmp
			awk -v p=$pair -v c=$command '/Throughput/ {print c,p,$4}' tmp >> measures	
		done
	done
done

rm tmp
