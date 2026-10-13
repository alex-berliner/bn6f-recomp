#!/bin/bash
# status.sh [RUN]: one screen of the driver's state (status.py --text).
exec bash ~/vidsync/recomp/tools/vidsync/remote/py.sh remote/status.py --text "$@"
