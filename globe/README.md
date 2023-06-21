This readme is to be used as an overview of the files/folders in "globe". The "app" folder contains its own README to describe the files more in depth.

app:
Includes c code for the config files, error handling, logging, peripheral management. The main loop is also included here.

board:
All the MM controller board related code can be found here. Including the adc, advertising, bluetooth, clocks, delays, i2c, pin definitions, power management, pwm, spi, timers, stack, uart and more.

bootloader:
nRF bootloader code (3rd party).
keys

drivers:
3rd party drivers for all peripherals including pressure sensor, fdoem, lc709203, maz31856, mcp3208, microsens, motion, sd card, sky66112, thermistor.

test: 
test code written to check various sensors, sd card, temp measurements etc.

3rd party:
Any other 3rd party library needed such as, arm-cmake-toolchains, fatfs, nRF5_SDK, bmi, cJSON, etc.

misc: 
Some code on the filesystem, cmake, debugging. Generally anything that does not fit in the folders above.

board.h is a header file with all the packages needed to run the board