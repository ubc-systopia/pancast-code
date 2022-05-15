#!/bin/bash

#######################################
## flash.sh
##
##     author: aasthakm
## created on: May 13, 2022
##
## script to flash zephyr beacon code
#######################################

sys="pancast"
zdev_str="zephyr-beacon"
zkit_str="NRF52832_xxAA_REV2"

rootdir=~/workspace-research/pancast/dev2
srcdir=$rootdir/pancast-code
z_beacondir=$srcdir/$zdev_str
outdir="$z_beacondir/build"
z_beaconimgdir="$z_beacondir/build/zephyr"
z_beaconimg="$z_beaconimgdir/$sys-$zdev_str.hex"


dev_list=( $(ls /dev/tty.usbmodem* | cut -d'/' -f3 | cut -d'.' -f2 | cut -b 9-21) )
num_devs=${#dev_list[@]}

dev_jlink_arr=()
for d in "${dev_list[@]}"; do
  dev_jlink_arr+=(${d:3:9})
done

#cd $rootdir/ncs/zephyr
#west flash -d $outdir <<< $jlink_id

function flash_zephyr_beacon_dev()
{
  jlink_id=$1

  cmd=""
  cmd=$cmd"cd $rootdir/ncs/zephyr; "
  cmd=$cmd"nrfjprog --program $z_beaconimg  \
    --sectoranduicrerase -f NRF52 --snr $jlink_id; "
  cmd=$cmd"nrfjprog -s $jlink_id --pinreset; "
  cmd=$cmd"cd -"
  (
   echo "$cmd"
   eval "$cmd"
  ) > zephyr$j.out 2>&1
}

for j in "${dev_jlink_arr[@]}"; do
  cmd="nrfjprog --deviceversion -s $j"
  res=$( eval "$cmd" )
  if [[ "$zkit_str" != "$res" ]]; then
    echo "Invalid device: $res"
    continue
  fi

  echo "$j: $zdev_str: $res"
  flash_zephyr_beacon_dev $j
done

#jlink_id='682213153'
#nrfjprog --program $outdir/zephyr/zephyr.hex  \
#  --sectoranduicrerase -f NRF52 --snr $jlink_id
#
#nrfjprog -s $jlink_id --pinreset

##nrfjprog -s 682213153 --program $outdir/zephyr/zephyr.hex \
##  --sectorerase -r
