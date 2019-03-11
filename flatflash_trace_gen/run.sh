#!/bin/bash

echo "Starting"

dist=uniform

for i in 128
do
    for j in 0.0 0.05 0.15 0.25 0.5 0.75 1.0
    do
	./master_trace_gen $i $j $dist > address/$dist-trace-$i-$j-changing-locality &
    done
done


wait
echo "Done"
