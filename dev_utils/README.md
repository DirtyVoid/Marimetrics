This readme is to be used as an overview of the files in dev_utils.

build.py:
This is a python script used for dictacting the firmware build. 

The hardware specifications for the controller on the board are specified in this folder, along with the bootloader start address, commands for cmake, build flags. This file also validates the public key to ensure the DFU package is accepted by the bootloader, and also has the function for creating DFU packages.

debug.py:
This python script is used to define functions for flashing and debugging the MM controller board.

Functions in this python script can get device id, get/set hardware revision, flash the controller, setup gdb, debug application.

image_hashes.py:
Generates and saves image hashes.

process.py:
defines functions to run and kill subprocess.