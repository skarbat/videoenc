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
#include <getopt.h>
#include "cliOptions.h"
#include <sstream>


#define ENC_VER "0.1.0"

//#define DUMP_OPTIONS 1

static CmdLineOptions default_options = {
  "-",
  1024,
  25,
  640,
  480,
  0,
  0,
  15,
  30,
  5
};

static struct option long_options[] =
{
    {"help",    no_argument,       0, 'h'},
    {"input",   required_argument, 0, 'i'},
    {"bitrate", required_argument, 0, 'b'},
    {"fps",     required_argument, 0, 'r'},
    {"size",    required_argument, 0, 's'},
    {"size-out", required_argument, 0, 'z'},
    {"qmin-max", required_argument, 0, 'q'},
    {"key-int", required_argument, 0, 'k'},
    {"output-list",   required_argument, 0, 'o'},
    {0, 0, 0, 0}
};



static  void help()
{
  printf( "H264 Encoder using H/W Accelerated For H3 SOCs - Version: %s\n", ENC_VER );
  printf( "Copyright (c) 2016 Rosimildo DaSilva - <rosimildo . at . gmail.com>\n" );
  printf( "Usage: videoenc <options>:\n" );
  printf( "   --input,-i     - input source ( default = [-] pipe source )\n" );
  printf( "   --bitrate,-b   - bit rate in Kbits/Sec ( default = 1024 )\n" );
  printf( "   --fps,-r       - frames per second ( default = 25 )\n" );
  printf( "   --size,-s      - video source size ( default = 640x480 )\n" );
  printf( "   --size-out,-z  - H264 video size output ( default = same as input )\n" );
  printf( "   --qmin-max,-q  - encoding quality ( default = -q 20,30 )\n" );
  printf( "   --key-int,-k   - key frmae interval ( default = 5 )\n" );
  printf( "   --output-list,-o - list of output FIFO's ( default = None, example: -o /tmp/o1.h264, /tmp/o2.h264, /tmp/o3.nv12 )\n" );
  printf( "\n\n" );
}

#ifdef DUMP_OPTIONS
static  void dumpOptions( CmdLineOptions & options )
{
  printf( "Input Src  [%s]\n", options.input.c_str() );
  printf( "Bit Rate   [%d]\n", options.bitrate );
  printf( "FPS        [%d]\n", options.fps );
  printf( "Width      [%d]\n", options.width );
  printf( "Height     [%d]\n", options.height );
  printf( "Width-Out  [%d]\n", options.width_out );
  printf( "Height-Out [%d]\n", options.height_out );
  printf( "qMin       [%d]\n", options.qMin );
  printf( "qMax       [%d]\n", options.qMax );
  printf( "KeyInterval[%d]\n", options.keyInterval );

  for( size_t i = 0; i < options.outFifos.size(); i++ )
  {
      printf( "Output FIFO[%d] = [%s]\n", i, options.outFifos[i].c_str() );
  }
}
#endif


int processCmdLineOptions( CmdLineOptions & options, int argc, char **argv )
{
   options  = default_options;
   int c;
   while( true )
   {
	  /* getopt_long stores the option index here. */
	  int option_index = 0;
	  c = getopt_long (argc, argv, "h:i:b:r:s:z:q:o:k:", long_options, &option_index);
	  /* Detect the end of the options. */
	  if (c == -1)
	    break;

	  switch (c)
	  {
	    case 'i':
	      options.input = optarg;
	      break;

	    case 'b':
	      options.bitrate = atoi( optarg );
	      break;

	    case 'r':
	      options.fps = atoi( optarg );
	      break;

	    case 'k':
	      options.keyInterval = atoi( optarg );
	      break;
      
	    case 'z':
	    case 's':
	    {
	      size_t w = 0, h = 0;
	      int n = sscanf( optarg, "%dx%d", &w, &h );
	      if( n == 2 )
	      {
		  if( c == 's' )
		  {
		     options.width  = w;
		     options.height = h;
		  }
		  else
		  {
		     options.width_out  = w;
		     options.height_out = h;
		  }
	       }
	    }
	    break;
	      
	    case 'q':
	    {
	      size_t qMin = 0, qMax = 0;
	      int n = sscanf( optarg, "%d,%d", &qMin, &qMax );
	      if( n == 2 )
	      {
	         options.qMin = qMin;
		 options.qMax = qMax;
	      }
	    }
	    break;

	    case 'o':
	    {
	       //printf ("List: %s\n", optarg );
	       std::stringstream ss( optarg );
	       while( ss.good() )
	       {
		   std::string substr;
		   std::getline( ss, substr, ',' );
		   options.outFifos.push_back( substr );
	       }
	    }
	    break;
	    
	    case 'h':
	    case '?':
	      //          help();

	    default:
	      help();
	      exit(2);
	 }
    }

   // fixup the out size,,, if not given...
    if( !options.width_out )
	 options.width_out  = options.width;
    if( !options.height_out )
         options.height_out = options.height;

   if( options.outFifos.size() < 1 )
   {
        help();
	exit(2);
   }    
 
#ifdef DUMP_OPTIONS
    dumpOptions( options );
#endif
    return 0;
}
