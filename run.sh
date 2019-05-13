#!/bin/bash

# Note: Mininet must be run as root.  So invoke this shell script
# using sudo.

time=150
dir=output
name=projectTest
trace=DL_2_16Mbps
loss=0.01

# project2 TCP
python run.py -tr $trace -t $time --name $name --dir $dir --loss $loss
python plot.py -tr $trace --name $name --dir $dir
