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

rootdir=$(pwd)
workdir="dongle-logs"

log_path="$rootdir/$workdir"
today=`date +%y%m%d-%H%M`
outdir="$log_path/$today"

mkdir -p $outdir

screen_log() {
  devstr=$( echo "$2" | cut -d'.' -f2 )
  devstr=${devstr:11:9}

  cmd="screen -d -m -L -Logfile $outdir/$1-$devstr.log $2"
  echo "$cmd"
  eval "$cmd"
  sleep 2
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
  ls ${dongles[$i]} 2> /dev/null 1>&2
  if [[ $? -eq 1 ]]; then
#    echo "[d$i] ${dongles[$i]} not found"
    continue
  fi

  screen_log "d$i" ${dongles[$i]}
done

screen -ls

echo "Press reset button on all dongles and enter y when done..."
confirm="n"

while [[ "$confirm" != "y" ]]; do
  ls -l "$outdir/"
  read -p "Pressed all buttons? Enter y or n: " confirm
done

sleep 1
confirm="n"

while [[ "$confirm" != "y" ]]; do
  read -p "Ready to kill screens? Enter y or n: " confirm
done

echo "Killing all screens ..."
killall screen
sleep 1
screen -ls
sleep 1
echo "Final logs: $outdir/"
ls -l "$outdir/"
