#!/bin/bash
pkill -f scripts/worker_server.py; sleep 2; pkill -f emu_server; echo "left $(pgrep -fc worker_server)"
