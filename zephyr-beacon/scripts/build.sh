#!/bin/bash

#######################################
## build.sh
##
##     author: aasthakm
## created on: May 13, 2022
##
## script to compile zephyr beacon code
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

#mkdir -p $outdir
cd $rootdir/ncs/zephyr
west build -c -p always -b nrf52dk_nrf52832 $z_beacondir  \
  -d $outdir -- -Wno-dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  > $z_beacondir/make.out

cp $z_beaconimgdir/zephyr.hex "$z_beaconimgdir/$sys-$zdev_str.hex"
