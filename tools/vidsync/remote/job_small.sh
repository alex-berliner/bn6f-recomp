#!/bin/bash
# the game screen at the GBA's size (as video_frame crops it), keyframes every 30 frames: fast full-size frames
cd ~/vidsync 2>/dev/null || cd /root/vidsync
nice -n 19 ffmpeg -v error -y -i data/full.mp4 -vf "crop=906:604:37:56,scale=240:160:flags=area" -c:v libx264 -preset veryfast -crf 6 -g 30 -pix_fmt yuv444p -an data/small.tmp.mp4 > data/small.log 2>&1 && mv data/small.tmp.mp4 data/small.mp4 && echo done >> data/small.log
