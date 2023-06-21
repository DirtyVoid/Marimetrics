import os
import atexit
import re
from .process import run, kill_process, root
from .build import target_paths, image_path, hardware
import subprocess
import signal

try:
    import gdb
except ModuleNotFoundError:
    pass


hardware_revision_to_hardware_type = {
    0: "nrf52832",
    1: "nrf52832",
    2: "nrf52832",
    3: "nrf52832",
    4: "nrf52840",
}

def nrfjprog(*args, capture_output=False):
    try:
        return run(["nrfjprog"] + list(args), capture_output=capture_output)
    except subprocess.CalledProcessError as err:

        messages = {
            33: "Can not communicate with target. Check debugger cables are not reversed.",
            41: "Can not connect to J-Link. Check USB connection from PC to J-Link.",
            43: ("Can not connect to target. "
                 + "Ensure target is powered and check connection between debugger and target."),
        }

        rc = err.returncode

        if rc in messages:
            raise RuntimeError(messages[rc]) from err
        raise

def get_device_id():
    device_id_str = nrfjprog("--memrd", "0x100000a4", "--n", "8", capture_output=True)
    m = re.match("0x[0-9a-fA-F]+: ([0-9a-fA-F]+) ([0-9a-fA-F]+)", device_id_str)
    return "02-" + hex(int((m[2] + m[1])[4:], 16) | (0x3 << 46))[2:]

def get_hardware_revision():
    memrd = nrfjprog("--memrd", "0x10001080", capture_output=True)
    rev = int(re.match("0x[0-9a-fA-F]+: ([0-9a-fA-F]+)", memrd)[1], base=16)
    if rev == 0xffffffff:
        raise RuntimeError("Hardware revision not set on target. " +
                           "Specifying the hardware revision while flashing the target.")
    return rev

def set_hardware_revision(revision):
    nrfjprog("--memwr", "0x10001080", "--val", hex(revision))

def hardware_revision_name_to_value(hardware_revision_name):
    if hardware_revision_name == None:
        return get_hardware_revision()
    return {
        "bare": 1,           # A bare-bones board with no external peripherals.
        "4.2": 2,
        "4.3": 3,             # updated peripheral power managment pin mapping.
        "dev": 4,
    }[hardware_revision_name]

def flash_target(build_type, hardware_revision_name=None, **target_paths_kwargs):
    hardware_revision = hardware_revision_name_to_value(hardware_revision_name)
    hardware_type = hardware_revision_to_hardware_type[hardware_revision]
    bl_path, bl_hex_path = target_paths("bootloader", hardware_type, build_type, **target_paths_kwargs)
    app_path, app_hex_path = target_paths("application", hardware_type, build_type, **target_paths_kwargs)

    image_path_ = image_path(hardware_type, bl_hex_path, app_hex_path)
    nrfjprog("-f", "nrf52", "--program", image_path_, "--chiperase")
    nrfjprog("--memwr", "0x10001080", "--val", hex(hardware_revision))
    nrfjprog("--reset")

    return hardware_type, bl_path, app_path

def gdb_flash(build_type):

    try:
        flash_target(build_type)
        # reload symbols
        gdb.execute("file " + gdb.current_progspace().filename)
        # clear source cache
        gdb.execute("directory")
        # reset the target
        gdb.execute("reset")
    except subprocess.CalledProcessError:
        pass

def setup_gdb(hardware_type, build_type):

    # Kill zombie processes.
    kill_process("JLinkGDBServer")
    kill_process("JLinkRTTClient")

    kwargs = {
        "text": True,
        "preexec_fn": os.setsid  # This prevents interrupts from being forwarded
    }


    gdbserver = subprocess.Popen(["JLinkGDBServer", "-device", hardware[hardware_type]["jlink"]["device"], "-If", "SWD", "-silent"], **kwargs)
    atexit.register(gdbserver.terminate)

    rttclient = subprocess.Popen(["JLinkRTTClient"], **kwargs)
    atexit.register(rttclient.terminate)

    gdb.execute("""
target remote localhost:2331

define reset
    monitor reset 2
end

define flash
    python dev_utils.debug.gdb_flash({build_type})
    # clear source cache
    directory
    # reset target
    reset
end

b main
b __assert_func

reset
""".format(build_type=repr(build_type)))

def debug_application(application_name):
    build_type = "dev"

    hardware_type, bl_path, app_path = flash_target(build_type)

    executable_path = {
        "bootloader": bl_path,
        "application": app_path,
    }[application_name]

    # The executable exists because the hex was built

    # Ignore all keyboard interrupts so they can be caught by GDB
    def signal_handler(sig, frame):
        pass
    signal.signal(signal.SIGINT, signal_handler)

    # Start GDB. This file is imported from within GDB and `setup_gdb` is run.
    run([
        "gdb-multiarch",
        "-ex", "python sys.path = [{}] + sys.path".format(repr(str(root))),
        "-ex", "python import dev_utils.debug",
        "-ex", "python dev_utils.debug.setup_gdb({hardware_type}, {build_type})".format(
            hardware_type=repr(hardware_type),
            build_type=repr(build_type)
        ),
        executable_path,
    ])
