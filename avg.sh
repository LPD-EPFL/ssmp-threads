#! /bin/sh

for command in "./threadone2one" "./one2one"
do
	echo $command
	for pair in 2 4 8 16 32 48 64 96 128 256 512 1024
	do
		awk -v e="$command $pair" ' BEGIN{sum = 0; count = 0} $0 ~ e {sum += $3; count++} END{print e, count, (sum/count)} ' measures 
	done
done

