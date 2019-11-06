#!/bin/bash
make clean
make
./utils/capture_packets.sh start
./server
./utils/capture_packets.sh stop
./utils/capture_packets.sh analyze > analysis.txt
