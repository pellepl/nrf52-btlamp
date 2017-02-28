#!/bin/bash

CFGFILE=$( readlink -f ./openocd/nordic_nrf52_dk_stlink.cfg )
sudo /home/petera/opt/openocd4all/oo4all -f interface/stlink-v2.cfg -f "$CFGFILE"
