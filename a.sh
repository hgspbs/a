#!/bin/sh

hwclk="hwclock -f /dev/rtc0 --noadjfile --localtime"
HWCLK="hwclock -f /dev/rtc1 --noadjfile --localtime"

function hwclktest()
{
  echo reset clock
  hwclock -f /dev/rtc1 --set --date="`hwclock -r -f /dev/rtc0`"
  $hwclk
  echo sleeping for 5 seconds
  sleep 5
  $hwclk
  $HWCLK
}

rmmod a

insmod a.ko
echo random
echo 0 > /proc/a
$HWCLK

echo slow 50%
echo -50 > /proc/a
hwclktest

echo fast 70%
echo 70 > /proc/a
hwclktest

echo rmmod
rmmod a
echo insmod
insmod a.ko

echo fast 30%
echo 30 > /proc/a
hwclktest

rmmod a
