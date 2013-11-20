#! /bin/sh

for command in "./threadclient_server" "./client_server"
do
	echo $command
	for core in 6 12 18 24 30 36 48 
	do
		for dsl in $((core/1)) $((core/2)) $((core/3)) $((core/4)) $((core/5)) $((core/6))
		do
			awk -v e="$command $core $dsl" 'BEGIN{sum = 0; count = 0} 
				$0 ~ e {sum += $5; count++} 
				END{if (count>0) {
				  	print e, (sum/count/1000000)
				}}' $1 
		done
	done
done

