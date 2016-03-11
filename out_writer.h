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

#ifndef __out_writer_h__
#define __out_writer_h__

#include <pthread.h>
#include <vector>
#include "SimpleFIFO.h"

#define QE_SIZE  20

struct  FIFO_Element
{
    FIFO_Element() : buf( 0 ), size( 0 ){} 
    FIFO_Element( void  *i_buf, int i_size ) : buf( i_buf ), size( i_size ) 
    {
    }
    void  *buf;
    int   size;
};

/**
 * This class push buffers to its respective FIFO's...
 */
class StreamWriterThread
{
public:

  StreamWriterThread( int buf_len, int fd = -1 );
  ~StreamWriterThread();

  void pushBuffer( void *buf0, int len0, void *buf1, int len1 );
  void end();
  void begin();
  void setNV12() { m_nv12=true; }
  bool isNV12()  const { return m_nv12;}

  void  setH264Header( unsigned char *buf, int len );
  void  pushHeader();
  bool openStream( const char *fn );
  
private:

  static void *thread_Out(void *parm);
  void threadLoop();

  bool  m_nv12;
  int   m_fd;
  bool  m_end;
  int   m_initLen;
  bool  m_hasBuf;
  pthread_cond_t   m_cond;
  pthread_mutex_t  m_mutex;
  pthread_t        threadid;

  SimpleFIFO< FIFO_Element, QE_SIZE> m_qFree;
  SimpleFIFO< FIFO_Element, QE_SIZE> m_qReady;

  unsigned char m_h264Header[32];
  int m_headerSize;
};

typedef std::vector< StreamWriterThread * > StreamerWriterVector;

#endif // __out_writer_h__
