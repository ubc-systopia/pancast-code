#!/usr/bin/env bash

# To collect logs from dongles:
# If using a new device, the device identifier should be added
# to the list of dongles below with the dongle id as key.
# Use USB connector to plug in 1 or more dongles to the PC. 
# Next, start the script and press the reset button on each dongle.
# The screen sessions for each device should run in the background.
# To view active screen sessions use `screen -ls` command.
# Unplug the dongle to kill the screen session.
# Logs will be saved at the path specified in screen_log()

rootdir=/Users/nboufford/workspace/pancast
workdir=/dongle-logs

log_path=$rootdir/$workdir
today=`date +%Y%m%dT%H-%M-%S`

dongle_dir() {
  if [ -d $log_path/$1 ]
  then
    :
  else
    echo "making new dongle dir"
    mkdir $1
  fi  
}

screen_log() {
  screen -d -m  -L -Logfile $log_path/$1/$today.log $2 &
}

declare -A dongles

dongles[d0]='/dev/tty.usbmodem0004401856281'
dongles[d1]='/dev/tty.usbmodem0004401848491'
dongles[d2]='/dev/tty.usbmodem0004401848771'
dongles[d3]='/dev/tty.usbmodem0004401853561'
dongles[d4]='/dev/tty.usbmodem0004401850581'
dongles[d5]='/dev/tty.usbmodem0004401851911'
dongles[d6]='/dev/tty.usbmodem0004401850921'
dongles[d7]='/dev/tty.usbmodem0004401856151'
dongles[d8]='/dev/tty.usbmodem0004401849221'
dongles[d9]='/dev/tty.usbmodem0004401843111'

for i in "${!dongles[@]}"
do
  ls ${dongles[$i]} 2> /dev/null
  if [[ $? -eq 1 ]]
  then
    echo "${dongles[$i]} not found"
  else
    dongle_dir $i
    screen_log $i ${dongles[$i]}
  fi   
done

