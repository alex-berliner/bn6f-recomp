#!/bin/bash
# job_small.sh: the game screen at the GBA's size (as video_frame crops it), keyframes every 30 frames: fast
# full-size frames (data/small.mp4). Once per PC, as a job: launch.ps1 -Job "job.sh small job_small.sh"
. ~/vidsync/recomp/tools/vidsync/remote/env.sh
nice -n 19 ffmpeg -v error -y -i data/full.mp4 -vf "crop=906:604:37:56,scale=240:160:flags=area" -c:v libx264 -preset veryfast -crf 6 -g 30 -pix_fmt yuv444p -an data/small.tmp.mp4 > data/small.log 2>&1 && mv data/small.tmp.mp4 data/small.mp4 && echo done >> data/small.log
