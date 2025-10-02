import datetime
import io
import os
import shutil
import zlib
import sys

SCRIPT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__)))

def parse_directory(directory):

    size = 0
    for root, dirs, files in os.walk(directory):
        for file in files:
            size += os.path.getsize(os.path.join(root, file))
    return size

def handler(event):
    key = event.get('key')
    download_path =  os.path.join(SCRIPT_DIR, key)
    size = parse_directory(download_path)

    compress_begin = datetime.datetime.now()
    shutil.make_archive(download_path, 'zip', root_dir=download_path)
    compress_end = datetime.datetime.now()

event = {}
event['key'] = sys.argv[1]
handler(event)
