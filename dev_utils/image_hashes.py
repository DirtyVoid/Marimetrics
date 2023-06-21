import hashlib
from .process import run, root
from .build import target_paths, application_hash_path, bootloader_hash_path

def generate_file_hash(input_file, output_path):
    h = hashlib.sha256()
    with open(input_file, "r") as f:
        h.update(f.read().encode())
    with open(output_path, "w") as f:
        f.write(h.hexdigest() + "\n")

def save_image_hashes():
    modified_files = list(filter(None, run(["git", "ls-files", "-m"], capture_output=True).split("\n")))
    # stash unstaged changes
    if modified_files:
        run(["git", "stash", "--keep-index", "--"] + modified_files)
    try:
        opts = {
            "hardware_type": "nrf52832",  # TODO: separate version for each hardware type
            "build_type": "prod",
            "use_debug_dfu_key": True,
            "static_build_settings": True,
        }
        bl_hex_path = target_paths("bootloader", **opts)[1]
        app_hex_path = target_paths("application", **opts)[1]
    finally:
        if modified_files:
            run(["git", "stash", "pop"])
    generate_file_hash(bl_hex_path, bootloader_hash_path)
    generate_file_hash(app_hex_path, application_hash_path)
    run(["git", "add", image_hash_dir])
