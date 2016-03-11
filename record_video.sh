#! /bin/bash

ENC_VIDEO="-re -f h264 -i /tmp/out1.h264 -flags +global_header"

LOGFILE="/tmp/record_video.log"
doLog()
{
  echo "`date` :  $1"
  echo "`date` :  $1" >> $LOGFILE
}

rm -f $LOGFILE

doLog 'Starting creating file'

ffmpeg -y $ENC_VIDEO -c:v copy -an -ss 5.000 -t "30.000" -f mp4 /tmp/result.mp4

doLog 'done creating file'
exit 0
