#! /bin/sh

for command in "./threadone2one" "./one2one"
do
	echo $command
	for location in "same-die" "same-mcm" "one-hop" "two-hop"
	do
		awk -v e="$command $location" ' BEGIN{sum = 0; count = 0} $0 ~ e {sum += $3; count++} END{print e, (sum/count)} ' measures 
	done
done

