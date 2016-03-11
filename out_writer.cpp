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
#include <unistd.h>
#include "out_writer.h"
#include <string>

#include <fcntl.h>
#include <stdint.h>


StreamWriterThread::StreamWriterThread( int buf_len, int fd )
: m_cond( PTHREAD_COND_INITIALIZER ), m_mutex( PTHREAD_MUTEX_INITIALIZER )
{
   m_fd      = fd;
   m_initLen = buf_len;
   m_hasBuf  = false;
   m_end     = false;
   threadid  = 0;
   m_nv12    = false;
   for( int i = 0; i < QE_SIZE; i++ )
   {
      m_qFree.enqueue( FIFO_Element( malloc( m_initLen ), m_initLen ) );
   }
   m_headerSize = 0;
}

StreamWriterThread::~StreamWriterThread()
{
  end();
  FIFO_Element myBuf;
  //printf( "Dtor - free = %d\n", m_qFree.count() );
  while( m_qFree.count() )
  {
      myBuf = m_qFree.dequeue();
      free( myBuf.buf );
  }
  //printf( "Dtor - ready = %d\n", m_qReady.count() );
  while( m_qReady.count() )
  {
      myBuf = m_qReady.dequeue();
      free( myBuf.buf );
  }
  if( m_fd != -1 )
  {
     close( m_fd );
     m_fd = -1;
  }
}

void StreamWriterThread::end()
{
    //printf( "end...\n" );
    if( !threadid ) return;
    m_end = true;
    pthread_cond_signal( &m_cond );
    pthread_join( threadid, NULL );
    threadid  = 0;
    //printf( "end done...\n" );
}



void StreamWriterThread::setH264Header( unsigned char *buf, int len )
{
    memcpy( m_h264Header, buf, len );
    m_headerSize = len;
}

void StreamWriterThread::pushHeader()
{
    //printf( "Push header: %d\n", m_headerSize );
    if( !isNV12() )
    {
        pushBuffer( m_h264Header, m_headerSize, 0, 0 );
    }
}


void StreamWriterThread::begin()
{
    m_end = false;
    pthread_create( &threadid, NULL, StreamWriterThread::thread_Out, this );
}

void StreamWriterThread::threadLoop()
{
   FIFO_Element myBuf;
   bool doBuffer = false;
   //printf( "thread loop starting\n" );
   while ( !m_end ) 
   {
         doBuffer = false;
       //printf( "loop locking\n" );
        pthread_mutex_lock( &m_mutex );
        /* 
         * Copy the buffer locally, since we might block
         * in case of a FIFO type of file output!
         */
        //printf( "loop waiting\n" );
        if( !m_qReady.count() )
	{
           pthread_cond_wait( &m_cond, &m_mutex );
        }
        if( m_qReady.count() )
	{
   	   myBuf = m_qReady.dequeue();
           doBuffer = true;
        }
        pthread_mutex_unlock( &m_mutex );
        //printf( "loop unlock\n" );
 	if( doBuffer )
	{
	    write( m_fd, myBuf.buf, myBuf.size );
            doBuffer = false;
            pthread_mutex_lock( &m_mutex );
            m_qFree.enqueue( myBuf );
            pthread_mutex_unlock( &m_mutex );
	}
   } 
}


void *StreamWriterThread::thread_Out(void *parm)
{
   StreamWriterThread *THIS = (StreamWriterThread*)parm;
   THIS->threadLoop();
   return NULL;
}


void StreamWriterThread::pushBuffer( void *buf0, int len0, void *buf1, int len1 )
{
   if(  !m_qFree.count() )
   {
     //printf( "F" );
        return;
   }
   /* Usually worker threads will loop on these operations */
   pthread_mutex_lock( &m_mutex );
   FIFO_Element myBuf;
   if( len0 )
   {
      myBuf = m_qFree.dequeue();
      memcpy( myBuf.buf, buf0, len0 );
      myBuf.size = len0;
      m_qReady.enqueue( myBuf );
   }
   if( len1 && m_qFree.count() )
   {
      myBuf = m_qFree.dequeue();
      memcpy( myBuf.buf, buf1, len1 );
      myBuf.size = len1;
      m_qReady.enqueue( myBuf );
   }
   pthread_cond_signal( &m_cond );
   pthread_mutex_unlock( &m_mutex );

 }

bool StreamWriterThread::openStream( const char *fn )
{
   m_fd = open( fn, O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
   std::string fnStr = fn;
   //printf( "Stream: %s\n", fn );
   if(fnStr.substr( fnStr.find_last_of(".") + 1) != "h264") {
       setNV12();
       //printf( "Stream: %s  IS nv12\n", fn );
   }
   return m_fd != -1;
}
