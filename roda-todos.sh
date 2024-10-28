#!/bin/bash
echo "USAGE: ./rodaTodos.sh <nElements>"
    
for i in {1..8}
do
    echo "Executando 10 vezes com $1 elementos e $i threads:"
    for j in {1..10}
    do
        ./reduceSumPth $1 $i | grep -oP '(?<=total_time_in_seconds: )[^ ]*'
    done
done





