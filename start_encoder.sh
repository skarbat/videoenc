#! /bin/bash

FFMPEG=/usr/local/bin/ffmpeg
SRC_VIDEO=/dev/video0
ROOT_DIR="`pwd`"

# ENC_TP="in-enc"
ENC_TP="v4l2-enc"

echo "Encoder Starting, ROOT=[$ROOT_DIR]"
while true
do
   
    case $ENC_TP in

	# FFMPEG as an Input Source to RAW video...
	'in-enc' )
            rm -rf /tmp/out*.h264 /tmp/out*.nv12
            mkfifo /tmp/out1.h264
            mkfifo /tmp/out2.nv12
            echo "Starting H264 Encoder..."
	    $FFMPEG -f v4l2 -input_format yuyv422 -r 25 -s 640x480 -i $SRC_VIDEO -pix_fmt yuv420p -an -r 25 -f rawvideo - | \
               $ROOT_DIR/videoenc -i - -k 2 -r 25 -b 1024 -s 640x480 -o /tmp/out1.h264
    	    ;;
	
	# using V4l2 as input source...
	'v4l2-enc' )
            rm -rf /tmp/out*.h264 /tmp/out*.nv12
            mkfifo /tmp/out1.h264
            mkfifo /tmp/out2.nv12
            echo "Starting H264 Encoder..."
	    $ROOT_DIR/videoenc -i $SRC_VIDEO -k 2 -r 25 -b 1024 -s 640x480 -o /tmp/out1.h264
    	    ;;
    esac
    sleep 5
    echo "Restarting..."
done
exit 0
