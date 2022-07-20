#!/usr/bin/bash

cd /home/pi/workspace/pancast-code/pi-client/src
sudo pkill -9 client
sleep 1
sudo ./client > ./client.log 2>&1 &
