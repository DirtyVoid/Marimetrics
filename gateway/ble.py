import asyncio
from bleak import BleakScanner, BleakClient, BleakError

# from .s3_upload import upload_record
from collections import namedtuple
import datetime
import inspect
import struct
import itertools
import random
import time
import contextlib
import re
import functools
import struct


def uuid(uuid16):
    return "8067{}-00bd-47bf-894a-24747d3c58df".format(uuid16.lower())


def nordic_uart_uuid(uuid16):
    return "6e40{}-b5a3-f393-e0a9-e50e24dcca9e".format(uuid16.lower())


SERVICE_UUID = uuid("00bd")
UART_SERVICE_UUID = nordic_uart_uuid("0001")
UART_RX_UUID = nordic_uart_uuid("0002")
UART_TX_UUID = nordic_uart_uuid("0003")

IS_LOGGING = uuid("2c7b")
SAMPLING_PERIOD = uuid("92f3")
TEMPERATURE = uuid("9eba")
CALIBRATION = uuid("e113")
LAST_O2_CALIBRATION = uuid("7fca")
LAST_PH_CALIBRATION = uuid("8c8e")
DEVICE_LABEL = uuid("a5dd")

TEMP_CALIB_0 = uuid("3d3c")
TEMP_CALIB_1 = uuid("f912")
TEMP_CALIB_2 = uuid("e7cf")

CALIBRATE_O2_0_PCT = 1
CALIBRATE_O2_100_PCT = 2
CALIBRATE_PH_LOW = 3
CALIBRATE_PH_HIGH = 4
CALIBRATE_PH_OFFSET = 5


def addr_to_device_id(addr):
    return "02-" + addr.replace(":", "").lower()


def device_id_to_addr(device_id):
    stripped_upper_id = device_id[3:].upper()
    left_nibbles = stripped_upper_id[0::2]
    right_nibbles = stripped_upper_id[1::2]
    return ":".join(l + r for l, r in zip(left_nibbles, right_nibbles))


def encode_value(value):
    if isinstance(value, int):
        return struct.pack("<Q", value)[:8]
    elif isinstance(value, str):
        return value.encode()
    elif isinstance(value, float):
        return struct.pack("<f", value)
    else:
        raise TypeError(value)


def decode_raw_value(pres_desc, value):
    fmt = pres_desc[0]
    if fmt == 1 or 2 <= fmt <= 11:
        # unsigned integer
        return int.from_bytes(value, byteorder="little", signed=False)
    if 12 <= fmt <= 19:
        # signed integer
        return int.from_bytes(value, byteorder="little", signed=True)
    if 20 <= fmt <= 21:
        # float
        return struct.unpack("f", value)[0]
    if fmt == 25:
        # string
        return value.decode().strip("\0")
    else:
        return "0x" + value.hex()


async def get_characteristic(client, characteristic):
    descriptors = {
        d.description: await client.read_gatt_descriptor(d.handle)
        for d in characteristic.descriptors
    }
    user_desc = descriptors["Characteristic User Description"].decode()
    pres_desc = descriptors["Characteristic Presentation Format"]

    raw_value = await client.read_gatt_char(characteristic)
    value = decode_raw_value(pres_desc, raw_value)

    return "{}: {}".format(user_desc, value)


async def try_connect(client):
    try:
        await client.connect()
        return True
    except (BleakError, asyncio.TimeoutError) as err:
        # print(addr, err)  # TODO: actually log this
        return False


async def anext(async_gen):
    return await async_gen.__anext__()


@contextlib.asynccontextmanager
async def retry_connect(addr):

    clients = (BleakClient(addr) for i in range(10))
    client = await anext(client for client in clients if await try_connect(client))

    try:
        yield client
    finally:
        await client.disconnect()


class UartProcessor:
    def __init__(self):
        self._buf = ""

    def has_partial_msg(self):
        return len(self._buf) > 0

    def append(self, data):
        self._buf += data.decode()
        parts = self._buf.split("\b")
        self._buf = parts[-1]
        msgs = parts[:-1]
        return msgs


@contextlib.asynccontextmanager
async def tx_uart_subscription(client, msg_handler):
    proc = UartProcessor()
    evt = asyncio.Event()
    waiting = [False]

    async def callback(sender, data):
        if waiting[0]:
            print(".", end="")
        msgs = proc.append(data)
        try:
            for msg in msgs:
                h = msg_handler(msg)
                if inspect.isawaitable(h):
                    await h
        finally:
            evt.set()

    await client.start_notify(UART_TX_UUID, callback)

    try:
        yield

        # Don't wait for the msg if there was a thrown exception to prevent
        # hanging. Could update this to have a disconnect callback on the client.

        if proc.has_partial_msg():
            print("Waiting for data dump", end="")

        waiting[0] = True
        while proc.has_partial_msg():
            await evt.wait()
            evt.clear()
    finally:
        print("")
        await client.stop_notify(UART_TX_UUID)


class Logging:
    def __init__(self):
        self._num_expected_msgs_remaining = 0
        self._cond = asyncio.Condition()

    async def enable(self, client):
        await client.write_gatt_char(IS_LOGGING, b"\x01")

    async def disable(self, client):
        async with self._cond:
            await client.write_gatt_char(IS_LOGGING, b"\x00")
            self._num_expected_msgs_remaining += 1

    async def notify_msg_received(self):
        async with self._cond:
            if self._num_expected_msgs_remaining > 0:
                self._num_expected_msgs_remaining -= 1
                self._cond.notify()

    async def wait_for_remaining_expected_msgs(self):
        async with self._cond:
            while self._num_expected_msgs_remaining > 0:
                await self._cond.wait()


async def test_device(addr):
    device_id = addr_to_device_id(addr)
    async with retry_connect(addr) as client:

        logging = Logging()

        async def msg_handler(msg):
            await logging.notify_msg_received()
            upload_record(device_id, msg)

        async with tx_uart_subscription(client, msg_handler):
            await logging.enable(client)
            print("Logging set")
            await asyncio.sleep(10)
            await logging.disable(client)
            print("Logging disabled")
            services = await client.get_services()
            service = services.get_service(SERVICE_UUID)
            characteristics = service.characteristics
            chars = [
                await get_characteristic(client, c) for c in service.characteristics
            ]
            print("\n".join([addr] + ["    " + c for c in chars]))
            await logging.wait_for_remaining_expected_msgs()
    return "OK"


async def process_scanned_devices(callback):
    scanner = BleakScanner()
    scanner.register_detection_callback(detection_callback)


async def scan_all_devices():
    scanned_devices = []
    evt = asyncio.Event()

    async def notify_scan(device):
        if device not in scanned_devices:
            scanned_devices.append(device)
            await asyncio.sleep(5.0)
            if scanned_devices[-1] == device:
                evt.set()

    def detection_callback_(device, advertising_data):
        asyncio.create_task(notify_scan(device))

    scanner = BleakScanner()
    scanner.register_detection_callback(detection_callback_)

    async def stop_when_nothing_scanned():
        await asyncio.sleep(5.0)
        if len(scanned_devices) == 0:
            evt.set()

    asyncio.create_task(
        stop_when_nothing_scanned()
    )  # Causes cancellation if nothing scanned

    await scanner.start()
    try:
        await evt.wait()
    finally:
        await scanner.stop()

    devices = (
        await scanner.get_discovered_devices()
    )  # More reliable than scanned_devices for some reason

    return [dev for dev in devices if SERVICE_UUID in dev.metadata.get("uuids", [])]


async def scan_addrs():
    return [dev.address for dev in await scan_all_devices()]


def pick_n(lst, n):
    return [lst[random.randrange(len(lst))] for _ in range(n)]


async def run_continuous_tests(avg_test_time_sec, tests_per_scan=10):
    t0 = time.time()

    scanned_addrs = await scan_addrs()
    print(scanned_addrs)
    test_addrs = pick_n(scanned_addrs, tests_per_scan)

    avg_time_between_consecutive_tests = (
        len(test_addrs) / len(scanned_addrs)
    ) * avg_test_time_sec
    lambd = 1 / avg_time_between_consecutive_tests

    # These are the times at which each test is scheduled to be conducted
    test_times = itertools.accumulate(
        (random.expovariate(lambd) for _ in range(len(test_addrs))), initial=t0
    )

    async def test_at_scheduled_time(t, addr):
        await asyncio.sleep(t - time.time())
        await test_device(addr)

    results = [
        await test_at_scheduled_time(t, addr) for t, addr in zip(test_times, test_addrs)
    ]
    results = [(addr, r) for addr, r in zip(test_addrs, results)]
    print(results)


def run_in_event_loop(f):
    @functools.wraps(f)
    def wrapper(*args, **kwargs):
        loop = asyncio.get_event_loop()
        return loop.run_until_complete(f(*args, **kwargs))

    return wrapper


async def start_logging(client):
    pass


async def stop_logging(client):
    pass


async def collect_scanned_devices(scan_filters):
    if not all(scan_filters):
        return []
    devices = await scan_all_devices()
    return [dev for dev in devices if all(filt(dev) for filt in scan_filters)]


@run_in_event_loop
async def ble(*directives):
    def filter_directives_by_attr(attr):
        attrs = [d[attr] for d in directives if attr in d]
        return attrs

    def any_of(attr):
        return len(filter_directives_by_attr(attr)) > 0

    def none_of(attr):
        return not any_of(attr)

    if none_of("scan_filter") and any_of("addrs"):
        scan_filters = [False]
    else:
        scan_filters = filter_directives_by_attr("scan_filter")

    explicit_addrs = filter_directives_by_attr("addrs")
    connected_device_operators = filter_directives_by_attr("connected_device_operator")

    print("scan_filters", scan_filters)
    print("explicit_addrs", explicit_addrs)
    print("connected_device_operators", connected_device_operators)

    if not all(scan_filters):
        scanned_devices = []
    else:
        print("scanning for devices...")
        scanned_devices = [
            dev
            for dev in await scan_all_devices()
            if all(filt(dev) for filt in scan_filters)
        ]
        print("scanned {} devices".format(len(scanned_devices)))

    devices = {}
    for addrs in explicit_addrs:
        devices.update({addr: None for addr in addrs})

    devices.update({dev.address: dev for dev in scanned_devices})

    for addr, scanned_device in devices.items():

        device_id = addr_to_device_id(addr)
        print(device_id)

        if len(connected_device_operators) > 0:
            try:
                print("connecting to {}...".format(device_id))
                async with retry_connect(addr) as client:
                    print("connected")

                    def msg_handler(msg):
                        upload_record(device_id, msg)

                    async with tx_uart_subscription(client, msg_handler):
                        for op in connected_device_operators:
                            r = op(client)
                            if inspect.isawaitable(r):
                                await r
                            await asyncio.sleep(2.0)
            except Exception as ex:
                print(ex)


def make_directive(name, f):
    def wrapper(*args, **kwargs):
        async def directive_function(client):
            await f(client, *args, **kwargs)

        directive = {}
        directive[name] = directive_function
        return directive

    return wrapper


def scan_filter(f):
    return make_directive("scan_filter", f)


def scanned_device_operator(f):
    return make_directive("scanned_device_operator", f)


def connected_device_operator(f):
    return make_directive("connected_device_operator", f)


@connected_device_operator
async def list_characteristics(client):
    services = await client.get_services()
    service = services.get_service(SERVICE_UUID)
    characteristics = service.characteristics
    chars = [await get_characteristic(client, c) for c in service.characteristics]
    print("\n".join(["    " + c for c in chars]))


def device_ids(*values):
    return {"addrs": [device_id_to_addr(addr) for addr in values]}


@connected_device_operator
async def noop(client):
    pass


def now():
    return int(
        1000
        * (
            datetime.datetime.now() - datetime.datetime.utcfromtimestamp(0)
        ).total_seconds()
    )


def encoded_now():
    return encode_value(now())


@connected_device_operator
async def disable_logging(client):
    await client.write_gatt_char(IS_LOGGING, b"\x00")


@connected_device_operator
async def enable_logging(client):
    await client.write_gatt_char(IS_LOGGING, encoded_now())


@connected_device_operator
async def finalize_o2_calibration(client):
    await client.write_gatt_char(LAST_O2_CALIBRATION, encoded_now())


@connected_device_operator
async def finalize_ph_calibration(client):
    await client.write_gatt_char(LAST_PH_CALIBRATION, encoded_now())


@connected_device_operator
async def write_characteristic(client, characteristic, value):
    await client.write_gatt_char(characteristic, encode_value(value))
