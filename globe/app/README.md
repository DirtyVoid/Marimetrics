This readme is to be used as an overview of the files/folders in "app".

config.c:
This file has functions to open/load/read/save/parse/debug the config file.

config.h:
Contains the defualt parameters of the config file.

error_handling.c:
When a fault is triggered, the function in this fil logs the fault in error_log.txt

logging.c:
Definitions of various functions used to write to the SD card, prints the headrs of the log files, 

peripheral_management.c: 
initializes the SD card, bmx160, fdoem. The initializations are in a specific order and only if the peripheral is active in the config file. The timing of the initialazation was determined empirically.

main.c:
This is the core main loop of the globe. The following is handled in this file:
app refresh, log_dumps, csv_printer, motion readings, temperature readings, sensor readings, writing to log files, set logging state, update sampling rate, update label, fdoem calibration, temperature calibration, salinity updates, reference pH updates, reading battery voltages, calibrate fuel guage, refreshing the app, motion logging.

The main loop initializes the board, then checks to ensure the globe is above the minimum voltage before initializing the ble characteristics. Once all peripherals are started with no faults, the main loop waits for events. These events could be user inputs from the app or measurement events based on the sampling interval.