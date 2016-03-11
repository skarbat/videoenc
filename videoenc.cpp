/*
 * Copyright (c) 2016 Rosimildo DaSilva <rosimildo@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <signal.h>
#include <string>

#include "aw/vencoder.h"
#include "out_writer.h"
#include "cliOptions.h"

// Hold command line options values...
static CmdLineOptions g_options;

unsigned int mwidth  = 640;
unsigned int mheight = 480;
unsigned int input_size  = mwidth * (mheight + mheight / 2);
int Y_size   = mwidth * mheight;
int UV_size  = mwidth * mheight / 2;

int g_msDelay = 0;

VencHeaderData  sps_pps_data;
VencH264Param   h264Param;
MotionParam     motionParam;
VENC_CODEC_TYPE codecType = VENC_CODEC_H264;

StreamerWriterVector writers;

#define MOTION_FLAG_FILE "/tmp/motion_sense"

#define ENCODE_H264 

typedef struct Venc_context
{
   VideoEncoder  *pVideoEnc;
   VencBaseConfig base_cfg;
   pthread_t thread_enc_id;
   pthread_t thread_pipe_id;
   int mstart;
   FILE *out_file;
   int  fd_in;
} Venc_context;


SimpleFIFO< VencInputBuffer, 10> g_inFIFO;
pthread_cond_t   g_cond( PTHREAD_COND_INITIALIZER );
pthread_mutex_t  g_mutex( PTHREAD_MUTEX_INITIALIZER );

static int read_file_data(int fd, void *buffer, int size)
{
   int total = 0, len;
   while (total < size)
   {
     len = read(fd, (unsigned char *)buffer + total, size - total);
      if (len == 0)
	return 0;
      total += len;
   }
   return total;
}

static void* file_thread(void* pThreadData) {

	Venc_context *venc_cxt = (Venc_context *)pThreadData;
	printf( "Pipe thread running: %p\n", venc_cxt );
	VideoEncoder  *pVideoEnc = venc_cxt->pVideoEnc;
        int fdIn = venc_cxt->fd_in;
	VencInputBuffer input_buffer;
        printf("FD IN = %d\n", fdIn );
 
	while(venc_cxt->mstart)
	{
	   int result = GetOneAllocInputBuffer( pVideoEnc, &input_buffer );
    	   if(result < 0) {
		printf("Alloc input buffer is full , skip this frame\n");
	        sleep(1);
		continue;
  	   }
	   unsigned int size;
	   size =  read_file_data( fdIn, input_buffer.pAddrVirY, Y_size );
	   size += read_file_data( fdIn, input_buffer.pAddrVirC, UV_size );
           if( input_size != size )
	   {
	       if( !size )
	       {
  	           printf("End of File\n" );
 		   venc_cxt->mstart = 0;
               }
  	       printf("Pipe sizes dont match[%d, %d]\n", input_size, size );
  	       ReturnOneAllocInputBuffer( pVideoEnc, &input_buffer );
               sleep(1);
	       continue;
           }
           pthread_mutex_lock( &g_mutex );
           bool res = g_inFIFO.enqueue( input_buffer );
   	   if( res ) pthread_cond_signal( &g_cond );
           pthread_mutex_unlock( &g_mutex );
	   if( !res )
	   {
	      printf("Unable to queue Buffer\n" );
  	      ReturnOneAllocInputBuffer( pVideoEnc, &input_buffer );
	   }

	   // Very crude way to emulate FPS...
	   if( g_msDelay )
	   {
	       usleep(1000*g_msDelay);      
	   }

	}
	return (void*)0;
}

void process_in_buffer(Venc_context * venc_cxt, VencInputBuffer *input_buffer );

static void set_motion_flag()
{
    char buf[ 512 ];
    sprintf( buf, "touch %s", MOTION_FLAG_FILE );
    system(  buf );
}

void write_bitstream( VideoEncoder *pVideoEnc )
{
  static int keyCount = 0;
  int result = 0;
  VencOutputBuffer output_buffer;
  memset(&output_buffer, 0, sizeof(VencOutputBuffer));
  result = GetOneBitstreamFrame(pVideoEnc, &output_buffer);
  if(result == 0)
  {
      // write the out to the H264 outputs, and also
      // writes a header, for each 50 frames......
      for( size_t j = 0; j < writers.size(); j++ )
      {
         StreamWriterThread *wr = writers[j];
         if( !wr->isNV12() )
         {
	     if( output_buffer.nFlag & VENC_BUFFERFLAG_KEYFRAME )
	     {
	         keyCount++;
		 if( keyCount > 5 )
                 {
	             wr->pushHeader();
		     keyCount = 0;
		 }
             }
             wr->pushBuffer( (void*)output_buffer.pData0, output_buffer.nSize0, (void*)output_buffer.pData1, output_buffer.nSize1 );
	 }
      }
      
      int motion_flag = 0;
      //VideoEncGetParameter(pVideoEnc, VENC_IndexParamMotionDetectStatus, &motion_flag);
      if (motion_flag == 1)
      {
          set_motion_flag();
	  printf("motion_flag = %d\n", motion_flag);
      }
      FreeOneBitStreamFrame( pVideoEnc, &output_buffer );
  }
  else
  {
      printf("Error getting bitstream\n");
  }
}

void process_in_buffer(Venc_context * venc_cxt, VencInputBuffer *input_buffer ) {
	VideoEncoder     *pVideoEnc   = venc_cxt->pVideoEnc;
	int result = 0;
        unsigned int theWidth;
        unsigned int theHeight;

	//	VencOutputBuffer output_buffer;

	theWidth    = mwidth;
	theHeight   = mheight;

        //struct timeval tmNow;
	//gettimeofday (&tmNow, NULL );
	//input_buffer->nPts = 1000000*(long long)tmNow.tv_sec + (long long)tmNow.tv_usec;
	//input_buffer->nPts = 900000*(long long)tmNow.tv_sec + (long long)tmNow.tv_usec;
	//long long nPts = 1000000 * (long long)tmNow.tv_sec + (long long)tmNow.tv_usec;
	//nPts = 9*(nPts/10);
	//input_buffer->nPts  = nPts;
	//printf("Cam - flush...\n" );
	result = FlushCacheAllocInputBuffer(pVideoEnc, input_buffer);
	if(result < 0) {
	    printf("Flush alloc error.\n" );
	}
	result = AddOneInputBuffer(pVideoEnc, input_buffer);
	if(result < 0) {
	    printf("Add one input buffer\n" );
	}
	result = VideoEncodeOneFrame(pVideoEnc);
	//printf("Cam - encode res = %d\n", result );
        // Update any output with RAWVIDEO data from the camera
        // in NV12 format....
        for( size_t j = 0; j < writers.size(); j++ )
        {
           StreamWriterThread *wr = writers[j];
           if( wr->isNV12() )
           {
	       wr->pushBuffer( (void*)input_buffer->pAddrVirY, theWidth * theHeight * 3/2, 0, 0 );
	   }
        }
	AlreadyUsedInputBuffer(pVideoEnc, input_buffer);
	ReturnOneAllocInputBuffer(pVideoEnc, input_buffer);
	if(result == 0)
	{
	    write_bitstream( pVideoEnc );
            if( h264Param.nCodingMode==VENC_FIELD_CODING && codecType==VENC_CODEC_H264)
            {
	       write_bitstream( pVideoEnc );
	    }  
	}
	else
	{
	    printf("encoder fatal error\n");
	}
}

static void* encoder_thread(void* pThreadData) {

	Venc_context * venc_cxt = (Venc_context *)pThreadData;
	printf("encoder thread running....\n");

        VencInputBuffer input_buffer;
	
        // Make sure a motion is set at startup....
        set_motion_flag();
	bool doBuffer = false;
	while(venc_cxt->mstart)
	{
              doBuffer = false;
              pthread_mutex_lock( &g_mutex );
              if( !g_inFIFO.count() )
	      {
                 pthread_cond_wait( &g_cond, &g_mutex );
              }
              if(g_inFIFO.count() )
	      {
   	         input_buffer = g_inFIFO.dequeue();
                 doBuffer = true;
              }
              pthread_mutex_unlock( &g_mutex );

              if( doBuffer )
	      {
                 process_in_buffer( venc_cxt, &input_buffer );
	      }
	}
	return (void*)0;
}


static int quit = 0;

void handle_int(int n)
{
    printf("Quit handler - called\n");
    quit = 1;
}

int main( int argc, char **argv )
{
   int err = 0;
   err = processCmdLineOptions( g_options, argc, argv );
   VideoEncoder* pVideoEnc = NULL;
   VencBaseConfig baseConfig;
   VencAllocateBufferParam bufferParam;
   unsigned int src_width,src_height,dst_width,dst_height;

    mwidth  = g_options.width;
    mheight = g_options.height;
    src_width  = mwidth;
    src_height = mheight;

    dst_width  = g_options.width_out;
    dst_height = g_options.height_out;

    Y_size   = mwidth * mheight;
    UV_size  = mwidth * mheight / 2;

    printf( "Size Image = %dx%d\n", mwidth, mheight );
    printf( "Y_size = %d\n", Y_size );
    printf( "UV_size = %d\n", UV_size );

    //intraRefresh
    //VencCyclicIntraRefresh sIntraRefresh;
    //sIntraRefresh.bEnable = 1;
    //sIntraRefresh.nBlockNumber = 10;

    //fix qp mode
    //VencH264FixQP fixQP;
    //fixQP.bEnable = 1;
    //fixQP.nIQp = g_options.qMin;
    //fixQP.nPQp = g_options.qMax;
	
    //* h264 param
    h264Param.bEntropyCodingCABAC = 1;
    h264Param.nBitrate    = 1024*g_options.bitrate; /* bps */
    h264Param.nFramerate  = g_options.fps; /* fps */
    h264Param.nCodingMode = VENC_FRAME_CODING;
    //h264Param.nCodingMode = VENC_FIELD_CODING;
    h264Param.nMaxKeyInterval = g_options.keyInterval;
    h264Param.sProfileLevel.nProfile = VENC_H264ProfileMain;
    h264Param.sProfileLevel.nLevel   = VENC_H264Level31;
    h264Param.sQPRange.nMinqp = g_options.qMin;
    h264Param.sQPRange.nMaxqp = g_options.qMax;

    Venc_context *venc_cxt = (Venc_context *)malloc(sizeof(Venc_context));
    memset(venc_cxt, 0, sizeof(Venc_context));
    memset(&baseConfig, 0 ,sizeof(VencBaseConfig));
    memset(&bufferParam, 0 ,sizeof(VencAllocateBufferParam));

    baseConfig.nInputWidth  = src_width;
    baseConfig.nInputHeight = src_height;
    baseConfig.nStride      = src_width;
	
    baseConfig.nDstWidth    = dst_width;
    baseConfig.nDstHeight   = dst_height;
    baseConfig.eInputFormat = VENC_PIXEL_YUV420P;
    //baseConfig.eInputFormat = VENC_PIXEL_YUYV422;
	
    bufferParam.nSizeY = baseConfig.nInputWidth*baseConfig.nInputHeight;
    bufferParam.nSizeC = baseConfig.nInputWidth*baseConfig.nInputHeight/2;
    bufferParam.nBufferNum = 4;
    printf( "Creating Encode\n" );
    pVideoEnc = VideoEncCreate(codecType);
    printf( "Creating Encode: %p\n", pVideoEnc );

    int value;
    VideoEncSetParameter(pVideoEnc, VENC_IndexParamH264Param, &h264Param);
    value = 0;
    VideoEncSetParameter(pVideoEnc, VENC_IndexParamIfilter, &value);
    value = 0; //degree
    VideoEncSetParameter(pVideoEnc, VENC_IndexParamRotation, &value);

    //VideoEncSetParameter(pVideoEnc, VENC_IndexParamH264FixQP, &fixQP);
    //VideoEncSetParameter(pVideoEnc, VENC_IndexParamH264CyclicIntraRefresh, &sIntraRefresh);

    venc_cxt->base_cfg	= baseConfig;
    venc_cxt->pVideoEnc = pVideoEnc;

    VideoEncInit(pVideoEnc, &baseConfig);
    unsigned int head_num = 0;
    VideoEncGetParameter(pVideoEnc, VENC_IndexParamH264SPSPPS, &sps_pps_data);
    printf("sps_pps size :%d\n", sps_pps_data.nLength );
    for( head_num=0; head_num<sps_pps_data.nLength; head_num++)
    {
         printf("the sps_pps :%02x\n", *(sps_pps_data.pBuffer+head_num));
    }

    motionParam.nMotionDetectEnable = 1;
    motionParam.nMotionDetectRatio  = 1; /* 0~12, 0 is the best sensitive */
    //VideoEncSetParameter(pVideoEnc, VENC_IndexParamMotionDetectEnable, &motionParam );

    input_size  = mwidth * (mheight + mheight / 2);
    printf( "InputSize=%d\n", input_size );

    AllocInputBuffer(pVideoEnc, &bufferParam);

    for( size_t j = 0; j < g_options.outFifos.size(); j++ )
    {
           StreamWriterThread *stOut = new StreamWriterThread( input_size );
	   bool rc = stOut->openStream( g_options.outFifos[j].c_str() );
           if( !rc )
	   {
	       printf("Could not open %s file\n", g_options.outFifos[j].c_str() );
	       return EXIT_FAILURE;
           }
           writers.push_back( stOut );
           stOut->begin();
	   if( !stOut->isNV12() )
	   {
               stOut->pushBuffer( (void *)sps_pps_data.pBuffer, sps_pps_data.nLength, 0, 0 );
               stOut->setH264Header( sps_pps_data.pBuffer, sps_pps_data.nLength );
           }
           printf("Open [%s] Stream, nv12=%d \n", g_options.outFifos[j].c_str(), stOut->isNV12() );
     }

     printf("create encoder ok\n");

     venc_cxt->mstart = 1;

     if( g_options.input == "-" )
     {
         venc_cxt->fd_in = 0;  // pipe input.
     }
     else
     {
        if( g_options.fps )
	{
	   g_msDelay = 1000 / g_options.fps;
	   g_msDelay -= 5;
	}
        venc_cxt->fd_in = open( g_options.input.c_str(), O_RDWR );
     }
     
     /* create encode thread*/
     err = pthread_create(&venc_cxt->thread_pipe_id, NULL, file_thread, venc_cxt);
     if (err || !venc_cxt->thread_pipe_id) {
	printf("Create thread_pipe_id fail !\n");
	goto fail_out;
     }
     /* start encoder */
     venc_cxt->mstart = 1;
     /* create encode thread*/
     err = pthread_create(&venc_cxt->thread_enc_id, NULL, encoder_thread, venc_cxt);
     if (err || !venc_cxt->thread_enc_id) {
	printf("Create thread_enc_id fail !\n");
     }

     struct sigaction sigact;
     memset(&sigact, 0, sizeof(sigact));
     sigact.sa_handler = handle_int;
     while( !quit && venc_cxt->mstart )
     {
	sleep( 1 );
     }
     pthread_mutex_lock( &g_mutex );
     pthread_cond_signal( &g_cond );
     pthread_mutex_unlock( &g_mutex );
     /* stop encoder */
     venc_cxt->mstart = 0;
     if(venc_cxt->thread_enc_id !=0)
     {
        pthread_join(venc_cxt->thread_enc_id,NULL);
     }
fail_out:
     ReleaseAllocInputBuffer(pVideoEnc);
     VideoEncUnInit(pVideoEnc);
     VideoEncDestroy(pVideoEnc);
     venc_cxt->pVideoEnc = NULL;
     for( size_t j = 0; j < writers.size(); j++ )
     {
         delete writers[j];
     }
     free(venc_cxt);
     venc_cxt = NULL;
     return 0;
}
