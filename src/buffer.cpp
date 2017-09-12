/*
 * Copyright (C) 2016 David McKelvie.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <Arduino.h>
#include <stdint.h>
#include "buffer.h"

CircularBuffer::CircularBuffer()
{
   head = 0;         //set indexes
   tail = 0;
}

void CircularBuffer::begin(uint8_t *buffer, uint8_t size)
{
  this->buffer = buffer;
  this->size = size;
}

bool CircularBuffer::get(uint8_t *ptrChar)
{
  if (!buffer) return false;

  // is buffer empty
  if (head == tail) return false;

  *ptrChar = buffer[head++];   // get char

  // circulate buffer
  if (head == size) {
    head = 0;
  }

  return true;
}

//////////////////////////////////////////////////////////
///
///\brief   place data in buffer
///
///\param   data data to place in buffer
///\return  true on success
///
//////////////////////////////////////////////////////////
bool CircularBuffer::put(uint8_t data)
{
  if (!buffer) return false;

  //is buffer full
  if (head == ((tail + 1) % size)) return false;

  buffer[tail] = data;      // put char

  // circulate  buffer
  if (++tail == size) {
    tail = 0;
  }

  return true;
}
