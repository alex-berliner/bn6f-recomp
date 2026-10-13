#!/bin/bash
pkill -f supervise.sh; pkill -f scripts/autosync.py; sleep 2; pkill -f emu_server; echo "left $(pgrep -fc scripts/autosync.py)"
