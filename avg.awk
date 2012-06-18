#!/usr/bin/awk -f

// {
    for (i = 0; i < NF; i++) {
	sum[i]+=$i;
    }
}

END {
    for (i = 0; i < NF; i++) {
	print i " : avg = " sum[i] / (NR-1)
	tsum += sum[i]
    }
    print "TOTAL avg: " tsum / (NR*(NR-1))
}