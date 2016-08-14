#!/bin/bash
#
#  Simple quick test - captures video from camera,
#  outputs a H264 encoded file.
#

h264out=$1

if [ "$h264out" == "" ]; then
    echo "Name of output H264 video file?"
    exit 1
fi

touch $h264out
sudo videoenc -i /dev/video0 -k 3 -r 25 -b 1024 -s 640x480 -o $h264out
