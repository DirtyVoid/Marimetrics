This readme is to be used as an overview of the files in gateway.

ble.py:
The service and characteristic uuids are defined in this script. A handful of useful conversion functions can be found here as well such as address to device id, device id to address, encode and decode bluetooth values, processing of UART messages,

Other functions include:
get_characteristics - returns all characteristics of the connected device.
try_connect - try to connect to a device, returns an error if fails.
retry_connect - try to connect to a device 10 times.
class Logging - class used to define functions for enabling and disabling logging.
test_device - a function for a partial automated test (unused)
scan_all_devices - scans for devices and stores the uuid, scans until no more devices can be found.
scan_addrs - returns the address for all devices scanned in "scan_all_devices"
run_continuous_test - automated test designed to scan all devices in range, runs the "test_device" function.

ble - function that enables running the specified command (list characteristic, start logging, calibrate, etc) on every device in range.


s3_upload.py:
This python script is used to upload the records to s3. File name for uploaded data is also created in the "upload record" function.