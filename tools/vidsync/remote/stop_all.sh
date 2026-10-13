#!/bin/bash
pkill -f "scripts/sec_search.py"; pkill -f "scripts/wait_filter2.py"; pkill -f "job_wf2"; sleep 1; echo "left: sec $(pgrep -fc scripts/sec_search.py) wf2 $(pgrep -fc scripts/wait_filter2.py)"
