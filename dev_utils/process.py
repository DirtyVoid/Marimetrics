import subprocess
from pathlib import Path

try:
    root = Path(__file__).parent.parent
except NameError:
    root = Path(".")
root = root.resolve()

def run(args, cwd=root, check=True, text=True, capture_output=False, **kwargs):
    res = subprocess.run([str(arg) for arg in args], cwd=str(cwd), check=check, text=text, capture_output=capture_output, **kwargs)
    return res.stdout if capture_output else None

def kill_process(process_name):
    try:
        run(["killall", process_name])
    except subprocess.CalledProcessError:
        # Will fail if the process is not found. Good.
        pass
