#!/bin/bash

if test "$#" -ne 1; then
	echo "USAGE: ./test_all.sh traces/TRACE"
	echo "Put the trace file directly with the dpc.gz extension"
else
	TRACE="$1"
	OUTPUT="output_tmp.txt"

	echo "########################################"
	echo "ampm_lite_prefetcher"
	zcat "$TRACE" | ./dpc2sim_stream_prefetcher > $OUTPUT 
	tail -n 3 $OUTPUT | head -n 1
	echo ""

	echo "########################################"
	echo "ip_stride_prefetcher"
	zcat "$TRACE" | ./dpc2sim_ip_stride_prefetcher > $OUTPUT 
	tail -n 3 $OUTPUT | head -n 1
	echo ""

	echo "########################################"
	echo "next_line_prefetcher"
	zcat "$TRACE" | ./dpc2sim_next_line_prefetcher > $OUTPUT 
	tail -n 3 $OUTPUT | head -n 1
	echo ""

	echo "########################################"
	echo "stream_prefetcher"
	zcat "$TRACE" | ./dpc2sim_stream_prefetcher > $OUTPUT 
	tail -n 3 $OUTPUT | head -n 1
	echo ""

	echo "########################################"
	echo "skeleton"
	zcat "$TRACE" | ./dpc2sim_skeleton > $OUTPUT 
	tail -n 3 $OUTPUT | head -n 1
	echo ""

	echo "########################################"
	echo "marc"
	zcat "$TRACE" | ./dpc2sim_marc_prefetcher > $OUTPUT 
	tail -n 3 $OUTPUT | head -n 1

	rm $OUTPUT
fi
