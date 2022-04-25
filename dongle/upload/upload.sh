#!/bin/bash

server_host="pancast.cs.ubc.ca"
server_port=443
defaultlog="screenlog.0"

usbstr="/dev/tty.usbmodem"
#echo ${#usbstr}

devstr=$( ls ${usbstr}* )
#echo ${devstr:${#usbstr}:13}
devid=${devstr##*$usbstr}

dir=$( pwd )
devfile="out${devid}"
devlog="$dir/${devfile}.txt"

function do_startcollect()
{
  cmd="script -t 0 ${devlog} screen -dmS ${devfile} ${devstr}"
  echo "$cmd"
  eval "$cmd"
  cmd="screen -ls"
  eval "$cmd"
  cmd="screen -S ${devfile} -p 0 -X log"
  echo "$cmd"
  eval "$cmd"
  echo "(1) Press reset button on your dongle ..."
  echo "(2) Wait for a minute to ensure all logs captured..."
}

### after upload is done, cause screen to quit
function do_endcollect()
{
  cmd="screen -XS ${devfile} quit"
  echo "$cmd"
  eval "$cmd"
}

function do_upload()
{
  cmd="mv ${defaultlog} ${devlog}"
  echo "$cmd"
  eval "$cmd"
  cmd="python3 upload.py -s ${server_host} -p ${server_port} -i ${devlog}"
  echo "$cmd"
  eval "$cmd"
}


### first press dongle button to re-print the log
echo "Enter input: startcollect | endcollect | upload"
while read line
do
  if [[ "$line" == "startcollect" ]]; then
    do_startcollect
    echo -e "\nEnter next input"
  elif [[ "$line" == "endcollect" ]]; then
    do_endcollect
    echo -e "\nEnter next input"
  elif [[ "$line" == "upload" ]]; then
    do_upload
    echo -e "\nEnter next input"
  elif [[ "$line" == "quit" ]] || [[ "$line" == "exit" ]]; then
    break
  else
    echo "Invalid input: $line."
  fi

done < "${1:-/dev/stdin}"

echo "Goodbye.."
