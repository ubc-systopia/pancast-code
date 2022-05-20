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
today=`date +%y%m%dT-%H%M`
outdir="$log_path/$today"

mkdir -p $outdir

screen_log() {
#  screen -d -m  -L -Logfile $log_path/$1/$today.log $2 &
  screen -d -m  -L -Logfile $outdir/$1.log $2 &
}

declare -a dongles
dongles=(
  '/dev/tty.usbmodem0004401843111'
  '/dev/tty.usbmodem0004401848491'
  '/dev/tty.usbmodem0004401848771'
  '/dev/tty.usbmodem0004401849221'
  '/dev/tty.usbmodem0004401850581'
  '/dev/tty.usbmodem0004401850921'
  '/dev/tty.usbmodem0004401851911'
  '/dev/tty.usbmodem0004401853561'
  '/dev/tty.usbmodem0004401856151'
  '/dev/tty.usbmodem0004401856281'
)

for i in "${!dongles[@]}"
do
  ls ${dongles[$i]} 2> /dev/null
  if [[ $? -eq 1 ]]; then
    echo "[d$i] ${dongles[$i]} not found"
    continue
  fi

  screen_log "d$i" ${dongles[$i]}
done

