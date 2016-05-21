Description
===========

This project provides a simple H264 Encoder Demo using Allwinner's SOC H/W H.264 encoder.
This version of the "encoder demo" uses the AW's BLOBs ( libraries ). Most of them can be
compiled from the archive provided by AW, except the encoder shared object.

Currently, this project has been tested on H3 H/W, more specifically OrangePI PC. It may work on other SOC's.

The AW "blobs" can be found on the archive called "liballwinner_linux.rar", which can be easily located with a google search using as
input the name of the archive. This archive has a lot of stuff in it.

     Filename for search: liballwinner_linux.rar

Also, the "encoder demo" provided on this repo, was used as well.

      https://github.com/allwinner-zh/media-codec


The only header provided by Allwinner, is placed in the directory called "aw".


The encoder has been programmed to require a "yuv420p" raw video as source:

    Source ( yuv420p ) ---> videoenc  ---> H264 stream


The link below has some files as raw YUV, which can be downloaded for test:

      http://trace.eas.asu.edu/yuv/


License
=======

All source is licensed under GPLv3, except the source released as demo by Allwinner.

All code in the following folders: "aw", "Camera", blobs and "watermark" have been released by AW as part of their demos,
and they follow whatever Allwinner's license is.

Compiling the Encoder
=====================

This test uses Armbian OS for H3, version 3.4.110+

Compilation is simple, grab the sources and:


First, install the provided BLOBs. They have been packaged in the "blobs" directory of the repository.

Go to:

	cd /
	tar xzvf [root_of_git_repo]/blobs/cedarx_blobs.tar.gz


Now, you need to config the ".so" libraries, adding:

    nano "/etc/ld.so.conf.d/cedarx.conf",  and the content below:

    # Cedarx configuration
    /usr/local/lib/cedarx

and once you save this file:

    ldconfig
    
Once this setup is done:

    make


This should compile and link, and you should get an utility called: "videoenc".

NOTE: at this stage, no "install" target, but maybe added later.


Testing the Encoder Demo
=======================

Make sure you have a V4L2 source, such as "/dev/video0"

  touch ./out1.h264
  ./videoenc -i /dev/video0 -k 3 -r 25 -b 1024 -s 640x480 -o ./out1.h264

This should get an output file "out1.h264" with the H.264 stream, which could be played on any player as long as it takes "h264" streams as input, for example ffplay or vlc.

If you got this far and it works, that is it!!!!! Congratulations!

The following is just groove!!!!!

This is the most common use case, where you have a V4l2 source, such as a Web Camera!


Testing with FFMPEG ( avconv )
=============================

The easier way is to make use of the Encoder Demo is to use "ffmpeg" ( avconv ) to convert input sources of video to the required format used by the Encoder ( yuv420p ).

So, you would need to get FFMPEG installed on your box. I've compiled from sources...., but you are free to install in any other way... Any recent FFMPEG would work.

Compile FFMPEG from sources:

   	   wget http://ffmpeg.org/releases/ffmpeg-2.8.6.tar.bz2
   	   tar xjvf ffmpeg-2.8.6.tar.bz2
   	   cd ffmpeg-2.8.6
   	   ./configure --enable-shared --prefix=/usr/local
   	   make
   	   make instal

A common use case of the encoder demo is to use "ffmpeg" to convert the input source to the required format ( yuv420p ),
including v4l2 based cameras. Example scripts are presented for that.

  Source --> FFMPEG -->( yuv420p ) --> videoenc --> H264 stream -->  FFMPEG  -->  MP4File ( or any other format ).

To aid test, I've added a few scripts that helps with test of a Web Camera.

start_stream.sh - this script uses FFMPEG to read a vl42 source ( web camera ), and converts its raw format to "yuv420p" which would be consumed by the encoder.
                  NOTE: Added to this script the capability to take as input a V4L2 source.

record_video.sh -- this script reads the output of the H264 stream, and mux it as a MP4 file for 30 seconds.

stream_video.sh -- this scripts stream the H264 stream live to an RTMP server.  You can use whatever provider ( server ) you are able to use.


Example with a Web Camera
=========================

Assuming the camera is "/dev/video0"

NOTE: Current mode in "start_encoder.sh" script is to use the V4L2 mode. 

Step 1: Check the input format and sizes provided:

     ffmpeg -f v4l2 -list_formats all -i /dev/video0

The command above should provided something like:

     [video4linux2,v4l2 @ 0x27df1b0] Raw       :     yuyv422 :     YUV 4:2:2 (YUYV) : 640x480 352x288 320x240 176x144 160x120 800x600 1280x720 1280x960 1280x1024 1600x1200
     [video4linux2,v4l2 @ 0x27df1b0] Compressed:       mjpeg :                MJPEG : 640x480 352x288 320x240 160x120 800x600 1280x720 1280x960 1280x1024 1600x1200


So, let's pick "yuyv422" as input format and size 640x480.

The "start_encoder.sh" is set for this.... please edit as needed.

Step 2: now, open a terminal shell and:

     ./start_encoder.sh

Step 3: go to another shell, and:

   ./record_video.sh

this should create a "mp4" video file ( /tmp/result.mp4 ), with a video stream of 30 seconds duration.

    /tmp/result.mp4

Step 4: Copy it to some machine with a player, like ffplay or VLC and enjoy!


Feedback
========

The project is at early state, but I am releasing as soon as possible to get feedback.
