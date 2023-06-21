
# Preparing the Development Environment
The firmware build has been tested on Ubuntu. You will need to install an CMake, ARM cross-compiler, GDB with multi-arch support, Segger J-Link libraries, and some Nordic nRF development programs.

On Ubuntu, you can install CMake, the cross-compiler and GDB with

```
sudo apt install cmake gdb-multiarch gcc-arm-none-eabi
```

You can find the J-Link libraries on the [Segger website](https://www.segger.com/downloads/jlink/) and the Nordic command line tools from the [Nordic website](https://www.nordicsemi.com/Software-and-tools/Development-Tools/nRF-Command-Line-Tools/). You will also need to install the following Python packages:

```
pip3 install nrfutil bleak
```

After you clone the project, use the top-level Python script to start interacting with the target and build system. e.g.

```
git clone git@bitbucket.org:marimetrics/firmware.git
cd firmware
chmod +x dev-utils
./dev-utils --help
```
