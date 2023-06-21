import boto3
from pathlib import Path
from datetime import datetime
import tempfile

s3 = boto3.client('s3')

def upload_record(device_id, data):
    path = Path(device_id) / datetime.now().strftime("%Y_%m_%d_%H_%M_%S.csv")
    with tempfile.TemporaryDirectory() as local_dir:
        local_path = Path(local_dir) / path
        local_path.parent.mkdir(parents=True, exist_ok=True)
        with open(local_path, "w") as fil:
            fil.write(data)
        s3.upload_file(str(local_path), 'uwq-test', str(path))
    return path
