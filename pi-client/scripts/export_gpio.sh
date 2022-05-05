#!/bin/bash

gpiopin=24
direction="out"
value=1

echo ${gpiopin} > /sys/class/gpio/unexport
echo ${gpiopin} > /sys/class/gpio/export
echo ${direction} > /sys/class/gpio/gpio${gpiopin}/direction
if [[ ${direction} == "out" ]]; then
  echo ${value} > /sys/class/gpio/gpio${gpiopin}/value
fi
