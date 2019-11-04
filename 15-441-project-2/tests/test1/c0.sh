#!/bin/bash

sudo tcset --overwrite --delay 20ms --rate 100Mbps --loss 0 enp0s8
tcshow enp0s8
