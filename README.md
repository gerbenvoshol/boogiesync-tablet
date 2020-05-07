# boogiesync-tablet
A userspace linux driver (written in C) for using the Boogie Board Sync 9.7 as a bluetooth tablet input device 

# Introduction

This is an implementation of an userspace linux driver for using the Boogie Board Sync 9.7 as a bluetooth tablet under linux.  There is only one apps for bluetooth.  It uses libbluetooth (bluez) to interface with the device and the UInput system (via libevdev) to generate input signals. Please note that this was quickly hacked together, so it may be unstable and full of bugs.

# Requirements

- gcc
- libbluetooth (bluez for bluetooth)
- libevdev


# Compilation

Compile with: gcc -o blue blue.c -lbluetooth -Wall -ggdb -levdev -I /usr/include/libevdev-1.0/

# Usage

To get bluetooth working, ./blue has to scan for your device. So, unless you know the address of the Boogie Board, you have to run ./blue. For the Boogie Board to be discoverable, you should have it turned off initially and turn the device on by pressing both the power and the save button at the same time. If this does not work, run ./blue with a specific address (e.g. ./blue A0:E6:F8:A1:72:DD) which you can discover using "hcitool scan" or any other tool. The program will then try to determine the channel to use. Sometimes sdp_connect fails, so it is also possible to add the channel as an argument to the program in case you know it already (e.g. ./blue A0:E6:F8:A1:72:DD 3).
