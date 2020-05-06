# boogiesync-tablet
A userspace linux driver (written in C) for using the Boogie Board Sync 9.7 as a bluetooth tablet input device 

# Introduction

This is an implementation of an userspace linux driver for using the Boogie Board Sync 9.7 as a bluetooth tablet under linux.  There is only one apps for bluetooth.  It uses libbluetooth (bluez) to interface with the device and the UInput system (via libevdev) to generate input signals. This is quickly hacked togehter, so it may be unstable and full of bugs.

# Requirements

- gcc
- libbluetooth (bluez for bluetooth)
- libevdev


# Compilation

Compile with: gcc -o blue blue.c -lbluetooth -Wall -ggdb -levdev -I /usr/include/libevdev-1.0/

# Usage

For bluetooth, it should scan for your device. For the Boogie Board to be discoverable, you should have it turned off and turn the device on by pressing the power and the save button simultaniously. If this does not work, run blue with a specific address (e.g., ./blue A0:E6:F8:A1:72:DD) which you can discover using "hcitool scan" or other tools.
