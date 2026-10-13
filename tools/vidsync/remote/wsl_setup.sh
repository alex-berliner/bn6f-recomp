#!/bin/bash
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq && apt-get install -y -qq libmgba-dev python3-numpy python3-scipy python3-pil build-essential > /tmp/apt.log 2>&1
echo "apt exit $?"; tail -2 /tmp/apt.log
python3 -c "import numpy, scipy, PIL; print('numpy', numpy.__version__, 'scipy', scipy.__version__)"
ls /usr/include/mgba/internal/gba/gba.h
. /etc/os-release; echo "$PRETTY_NAME"; nproc
