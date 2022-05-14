#!/bin/bash

rootdir=~/workspace-research/pancast/dev2
srcdir=$rootdir/pancast-code/zephyr-beacon
outdir=$srcdir/build

mkdir -p $outdir
cd $rootdir/ncs/zephyr
west build -c -p auto -b nrf52dk_nrf52832 $srcdir -d $outdir -- -Wno-dev -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > $srcdir/make.out
