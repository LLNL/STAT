/*
  Copyright (c) 2007-2013, Lawrence Livermore National Security, LLC.
  Produced at the Lawrence Livermore National Laboratory
  Written by Gregory Lee [lee218@llnl.gov], Dorian Arnold, Matthew LeGendre, Dong Ahn, Bronis de Supinski, Barton Miller, and Martin Schulz.
  LLNL-CODE-624152.
  All rights reserved.

  This file is part of STAT. For details, see http://www.paradyn.org/STAT/STAt.html. Please also read STAT/LICENSE.

  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, this list of conditions and the disclaimer below.
  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the disclaimer (as noted below) in the documentation and/or other materials provided with the distribution.
  Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "STAT_CircularLogs.h"

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>

CircularBuffer::CircularBuffer(size_t size) :
    buffer(NULL),
    fhandle(NULL),
    bufferSize(size)
{
    cookie_io_functions_t funcs;
    funcs.read = NULL;
    funcs.write = WriteWrapper;
    funcs.seek = NULL;
    funcs.close = CloseWrapper;
   
    fhandle = fopencookie(this, "w", funcs);
    if (!fhandle)
        return;
}

CircularBuffer::~CircularBuffer()
{
    if (fhandle)
        fclose(fhandle);
    if (buffer)
        delete buffer;
    fhandle = NULL;
    buffer = NULL;
}

void CircularBuffer::init()
{
    if (!buffer) {
        buffer = new buffer_t(bufferSize);
        if (!buffer) return;
    }
   
    if (!fhandle) {
        cookie_io_functions_t funcs;
        funcs.read = NULL;
        funcs.write = WriteWrapper;
        funcs.seek = NULL;
        funcs.close = CloseWrapper;
      
        fhandle = fopencookie(this, "w", funcs);
    }
}
ssize_t CircularBuffer::WriteWrapper(void *cookie, const char *buf, size_t size) {
    CircularBuffer *me = static_cast<CircularBuffer *>(cookie);
    return me->CWrite(buf, size);
}

int CircularBuffer::CloseWrapper(void *cookie)
{
    CircularBuffer *me = static_cast<CircularBuffer *>(cookie);
    return me->CClose();
}

ssize_t CircularBuffer::CWrite(const char *buf, size_t size) {
    for (unsigned i=0; i<size; i++) {
        buffer->push_back(buf[i]);
    }
    return size;
}

int CircularBuffer::CClose() {
    reset();
    fhandle = NULL;
    return 0;
}

FILE *CircularBuffer::handle() {
    init();
    return fhandle;
}

bool CircularBuffer::getBuffer(char* &buffer1, size_t &buffer1_size, char* &buffer2, size_t &buffer2_size)
{
   if (!buffer)
      return false;

   buffer1 = buffer->array_one().first;
   buffer1_size = buffer->array_one().second;
   buffer2 = buffer->array_two().first;
   buffer2_size = buffer->array_two().second;

   return true;
}

int CircularBuffer::flushBufferTo(int fd) {
   char *buffer1, *buffer2;
   size_t buffer1_size, buffer2_size;
   getBuffer(buffer1, buffer1_size, buffer2, buffer2_size);
   
   int numWritten = 0;
   if (buffer1) {
      int result = write(fd, buffer1, buffer1_size);
      if (result == -1)
         return -1;
      numWritten += result;
   }
   
   if (buffer2) {
      int result = write(fd, buffer2, buffer2_size);
      if (result == -1)
         return -1;
      numWritten += result;
   }
   
   return numWritten;
}

const char *CircularBuffer::str() {
   if (!buffer) 
      return "";

   buffer->push_back(0);
   return buffer->linearize();
}

void CircularBuffer::reset() {
   if (!buffer)
      return;
   buffer->resize(0);
}
