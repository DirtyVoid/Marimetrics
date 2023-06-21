from .process import run, root

bootloader_dir = root / "globe/bootloader"
softdevice_dir = (
    root / "globe/third_party/nRF5_SDK/nRF5_SDK_16.0.0/components/softdevice"
)

hardware = {
    "nrf52832": {
        "softdevice_hex_path": softdevice_dir
        / "s132/hex/s132_nrf52_7.0.1_softdevice.hex",
        "bootloader_start_addr": {
            "dev": "0x70000",
            "prod": "0x77000",
        },
        "nrfutil": {
            "--family": "NRF52",
            "--sd-req": "0xCB",
            "--sd-id": "0xCB",
        },
        "jlink": {
            "device": "nRF52832_xxAA",
        },
    },
    "nrf52840": {
        "softdevice_hex_path": softdevice_dir
        / "s140/hex/s140_nrf52_7.0.1_softdevice.hex",
        "bootloader_start_addr": {
            "dev": "0xF0000",
            "prod": "0xF7000",
        },
        "nrfutil": {
            "--family": "NRF52840",
            "--sd-req": "0xCA",
            "--sd-id": "0xCA",
        },
        "jlink": {"device": "nRF52840_xxAA"},
    },
}

toolchain_path = (
    root / "globe" / "third_party" / "arm-cmake-toolchains" / "arm-gcc-toolchain.cmake"
)


def target_paths(
    target_name,
    hardware_type,
    build_type,
    use_debug_dfu_key=None,
    static_build_settings=None,
):
    target_build_type = {
        ("dev", "application"): "Debug",
        ("dev", "bootloader"): "Debug",
        ("prod", "application"): "Release",
        ("prod", "bootloader"): "MinSizeRel",
    }[(build_type, target_name)]

    build_dir = root / "dev_utils" / "build" / hardware_type / target_build_type

    if not build_dir.exists():
        build_dir.mkdir(parents=True)

        bootloader_start_addr = hardware[hardware_type]["bootloader_start_addr"][
            build_type
        ]

        cmake_vars = [
            ("HARDWARE_TYPE", hardware_type),
            ("BOOTLOADER_START_ADDR", bootloader_start_addr),
        ]

        run(
            [
                "cmake",
                "-DCMAKE_TOOLCHAIN_FILE=" + str(toolchain_path),
                "-DCMAKE_BUILD_TYPE=" + target_build_type,
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            ]
            + ["-D{}:STRING={}".format(name, value) for name, value in cmake_vars]
            + [root],
            cwd=build_dir,
        )

    build_flags = [
        ("USE_DEBUG_DFU_KEY", use_debug_dfu_key),
        ("STATIC_BUILD_SETTINGS", static_build_settings),
    ]

    for name, value in build_flags:
        run(
            [
                "cmake",
                "-D",
                "{}:BOOL={}".format(name, "ON" if value else "OFF"),
                build_dir,
            ],
            cwd=build_dir,
        )

    run(
        ["cmake", "--build", build_dir, "--target", target_name, "--parallel"],
        cwd=build_dir,
    )

    app_path = build_dir / target_name
    assert app_path.exists()
    hex_path = build_dir / (target_name + ".hex")
    assert hex_path.exists()

    return app_path, hex_path


def image_path(hardware_type, bootloader_hex_path, application_hex_path):
    softdevice_hex_path = hardware[hardware_type]["softdevice_hex_path"]
    output_dir = application_hex_path.parent
    bl_sd_path = output_dir / "bl_sd.hex"
    image_path = output_dir / "image.hex"
    settings_path = output_dir / "settings.hex"

    # Note that the bootloader and application both specify start addresses
    # in their respective hex files. This will cause an error if they are
    # merged directly. To circumvent this issue, the softdevice and
    # bootloader are merged first and the bootloader's start address is
    # removed. The resulting hex file is then merged with the application
    # hex file, preserving the application start address.
    #
    # Note that on our architecture the start address specified in the hex
    # file does not matter.
    run(
        [
            "hexmerge.py",
            "--no-start-addr",
            softdevice_hex_path,
            bootloader_hex_path,
            "-o",
            bl_sd_path,
        ]
    )

    # Generate bootloader settings page
    run(
        [
            "nrfutil",
            "settings",
            "generate",
            "--family",
            hardware[hardware_type]["nrfutil"]["--family"],
            "--application",
            application_hex_path,
            "--application-version",
            app_version,
            "--bootloader-version",
            bootloader_version,
            "--bl-settings-version",
            "2",
            "--app-boot-validation",
            "NO_VALIDATION",
            "--sd-boot-validation",
            "NO_VALIDATION",
            "--softdevice",
            softdevice_hex_path,
            settings_path,
        ]
    )

    # Merge all parts
    run(
        [
            "hexmerge.py",
            bl_sd_path,
            application_hex_path,
            settings_path,
            "-o",
            image_path,
        ]
    )

    return image_path


def validate_pk(key_file, pk_file):
    # Generate the public key from the private key and make sure it matches.
    # This is just a fail safe to ensure that the DFU package will be accepted
    # by the bootloader.

    pk = run(
        ["nrfutil", "keys", "display", "--key", "pk", "--format", "pem", key_file],
        capture_output=True,
    )
    with open(pk_file, "r") as f:
        expected_pk = f.read()
    if pk.strip() != expected_pk.strip():
        raise ValueError("private key does not match public key")


def make_dfu_package(
    out_file,
    hardware_type,
    key_file=None,
    app_version_bump=False,
    bootloader_version_bump=False,
):

    spec = hardware[hardware_type]
    softdevice_hex_path = spec["softdevice_hex_path"]
    nrfutil = spec["nrfutil"]

    use_debug_dfu_key = key_file is None

    if use_debug_dfu_key:
        key_file = bootloader_dir / "debug.pem"
        pk_file = bootloader_dir / "debug_pk.pem"
    else:
        pk_file = bootloader_dir / "pk.pem"

    validate_pk(key_file, pk_file)

    opts = {
        "hardware_type": hardware_type,
        "build_type": "prod",
        "use_debug_dfu_key": use_debug_dfu_key,
        "static_build_settings": False,
    }

    bl_path, bl_hex_path = target_paths("bootloader", **opts)
    app_path, app_hex_path = target_paths("application", **opts)

    run(
        [
            "nrfutil",
            "pkg",
            "generate",
            "--application",
            app_hex_path,
            "--application-version",
            int(version) + int(app_version_bump),
            "--bootloader",
            bl_hex_path,
            "--bootloader-version",
            int(version) + int(bootloader_version_bump),
            "--hw-version",
            "0",
            "--sd-req",
            nrfutil["--sd-req"],
            "--sd-id",
            nrfutil["--sd-id"],
            "--softdevice",
            softdevice_hex_path,
            "--sd-boot-validation",
            "NO_VALIDATION",
            "--app-boot-validation",
            "NO_VALIDATION",
            "--key-file",
            key_file,
            out_file,
        ]
    )


revision = run(
    ["git", "describe", "--tags", "--dirty", "--abbrev=40"], capture_output=True
).strip()


def generate_build_settings(output_file):
    build_settings_content = """#ifndef BUILD_SETTINGS_H
    #define BUILD_SETTINGS_H
    #define GIT_REVISION {}
    #define GIT_VERSION_NUMBER {}
    #endif
    """.format(
        revision, version
    )

    # The file is only written if the contents have changed. This avoids
    # unnecessary linking.
    try:
        with open(output_file, "r") as fil:
            existing_content = fil.read()
    except FileNotFoundError:
        existing_content = None

    if build_settings_content != existing_content:
        with open(output_file, "w") as fil:
            fil.write(build_settings_content)


image_hash_dir = root / "dev_utils/image_hashes"
application_hash_path = image_hash_dir / "application.txt"
bootloader_hash_path = image_hash_dir / "bootloader.txt"


def count_rev_list(paths):
    return int(run(["git", "rev-list", "--count", "HEAD"] + paths, capture_output=True))


app_version = count_rev_list([application_hash_path]) + 45
bootloader_version = count_rev_list([bootloader_hash_path]) + 7
version = count_rev_list([application_hash_path, bootloader_hash_path]) + 45
