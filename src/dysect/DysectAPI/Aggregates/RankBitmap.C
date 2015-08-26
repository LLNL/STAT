/*
Copyright (c) 2013-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Jesper Puge Nielsen, Niklas Nielsen, Gregory Lee [lee218@llnl.gov], Dong Ahn.
LLNL-CODE-645136.
All rights reserved.

This file is part of DysectAPI. For details, see https://github.com/lee218llnl/DysectAPI. Please also read dysect/LICENSE

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <cstdlib>
#include <cstdio>

#include "DysectAPI/Aggregates/RankBitmap.h"

using namespace std;
using namespace DysectAPI;

int RankBitmap::BitmapPart::getArrIndex(int bitNr) {
  return (bitNr - startBitNr) / bitsInLong;
}

int RankBitmap::BitmapPart::getBitIndex(int bitNr) {
  return bitNr & (bitsInLong - 1);
}

int RankBitmap::BitmapPart::getStartBitNr(int bitNr) {
  return bitNr & ~(bitsInLong - 1);
}

long RankBitmap::BitmapPart::getBitMask(int bitNr) {
  return 0x1LL << getBitIndex(bitNr);
}

int RankBitmap::BitmapPart::getLastBit() {
  return startBitNr + (length * bitsInLong) - 1;
}

RankBitmap::BitmapPart::BitmapPart(RankBitmap::BitmapPart* next) {
  this->length = 0;
  this->startBitNr = 0;
  this->next = next;
  this->bits = 0;
}

RankBitmap::BitmapPart::~BitmapPart() {
  if (next != 0) {
    delete next;
  }

  if (bits != 0) {
    free(bits);
  }
}

void RankBitmap::BitmapPart::setBit(int bitNr) {
  // Check if this part of the bitmap is uninitialized
  if (bits == 0) {
    length = 1;
    startBitNr = getStartBitNr(bitNr);
    bits = (long*)malloc(sizeof(long) * length);
    bits[0] = getBitMask(bitNr);
    return;
  }

  // Check if the bit belongs before this part of the bitmap
  if (bitNr < startBitNr) {
    // Create a copy of this, and use it as the next
    BitmapPart* newNext = new BitmapPart();
    newNext->length = length;
    newNext->startBitNr = startBitNr;
    newNext->next = next;
    newNext->bits = bits;

    // Reuse the current part for the new bit
    next = newNext;
    bits = 0;
    setBit(bitNr);
    tryMergeNext();
    return;
  }

  // Check if the bit belong in this part of the bitmap
  if (bitNr <= getLastBit()) {
    bits[getArrIndex(bitNr)] |= getBitMask(bitNr);

    return;
  }

  // Check if the bit belong somewhere further down the chain
  if ((next != 0) && (next->startBitNr < bitNr)) {
    next->setBit(bitNr);

    return;
  }

  // Check if we should expand 
  if ((startBitNr + length * bitsInLong + expandLimit) > bitNr) {
    expand();

    setBit(bitNr);
    return;
  }

  // Create a new part of the bitmap for the value
  next = new BitmapPart(next);
  next->setBit(bitNr);
  next->tryMergeNext();
}

bool RankBitmap::BitmapPart::getBit(int bitNr) {
  if (// Is this part of the bitmap uninitialized
      (bits == 0) ||
      // Does the bit belong before this part of the bitmap
      (bitNr < startBitNr)) { 
    return false;
  }

  // Check if the bit belong in this part of the bitmap
  if (bitNr <= getLastBit()) {
    return bits[getArrIndex(bitNr)] & getBitMask(bitNr);
  }

  // Check if this is the end of the chain
  if (next == 0) {
    return false;
  }

  // Check the next list item
  return next->getBit(bitNr);
}

void RankBitmap::BitmapPart::expand() {
  expand(expandAmount);
}

void RankBitmap::BitmapPart::expand(int amount) {
  int newLength = length + amount;

  if (shouldMerge(newLength)) {
    mergeNext();
    return;
  }

  long* newBits = (long*)malloc(sizeof(long) * newLength);

  int i = 0;
  for (; i < length; i++) {
    newBits[i] = bits[i];
  }

  for (; i < newLength; i++) {
    newBits[i] = 0;
  }

  free(bits);

  length = newLength;
  bits = newBits;
}

bool RankBitmap::BitmapPart::shouldMerge(int atLength) {
  return ((next != 0) && ((startBitNr + atLength * bitsInLong + expandLimit) > next->startBitNr));
}

void RankBitmap::BitmapPart::tryMergeNext() {
  if (shouldMerge(length)) {
    mergeNext();
  }
}

void RankBitmap::BitmapPart::mergeNext() {
  if (next == 0) return;

  BitmapPart* newNext = next->next;
  int newLength = getArrIndex(next->getLastBit()) + 1;
  long* newBits = (long*)malloc(sizeof(long) * newLength);

  int i = 0;
  for (; i < length; i++) {
    newBits[i] = bits[i];
  }

  int nextStartIndex = getArrIndex(next->startBitNr);
  for (; i < nextStartIndex; i++) {
    newBits[i] = 0;
  }

  for (int j = 0; i < newLength; i++, j++) {
    newBits[i] = next->bits[j];
  }

  // Detach and remove the next
  next->next = 0;
  delete next;
  free(bits);

  length = newLength;
  bits = newBits;
  next = newNext;
}

int RankBitmap::BitmapPart::describe(string& res) {
  int count = 0;

  if (bits != 0) {
    int curRangeStart = -1;
    int curRangeEnd;
    int lastBit = getLastBit();
    int curBit = startBitNr;
    char buffer[32];

    for (int i = 0; i < length; i++) {
      long curPart = bits[i];

      for (int j = 0; j < bitsInLong; j++) {
        if (curPart & 0x1) {
          /* The bit is set, check if this is the start of a new range */
          if (curRangeStart == -1) {
            curRangeStart = curBit;
          }

          curRangeEnd = curBit;
          count += 1;
        } else {
          /* The bit is not set, check if a range has just ended */
          if (curRangeStart != -1) {
            snprintf(buffer, 32, "%d", curRangeStart);
            res.append(buffer);

            if (curRangeStart != curRangeEnd) {
              res.append("-");

              snprintf(buffer, 32, "%d", curRangeEnd);
              res.append(buffer);
            }

            res.append(",");

            curRangeStart = -1;
          }
        }

        curBit += 1;
        curPart = curPart >> 1;
      }
    }

    /* If the last bit was set, it has not been printed yet */
    if (curRangeStart != -1) {
      snprintf(buffer, 32, "%d", curRangeStart);
      res.append(buffer);

      if (curRangeStart != curRangeEnd) {
        res.append("-");

        snprintf(buffer, 32, "%d", curRangeEnd);
        res.append(buffer);
      }

      res.append(",");
    }
  }

  if (next != 0) {
    count += next->describe(res);
  }

  return count;
}

int RankBitmap::BitmapPart::listSize() {
  return (sizeof(int) * 3 + length * sizeof(long) + (next ? next->listSize() : 0));
}

int RankBitmap::BitmapPart::serialize(char* p) {
  int* header = (int*)p;

  header[0] = startBitNr;
  header[1] = length;
  header[2] = (next == 0 ? 0 : 1);

  long* data = (long*)(&header[3]);

  int i;
  for (i = 0; i < length; i++) {
    data[i] = bits[i];
  }

  return (sizeof(int) * 3 + length * sizeof(long) + (next ? next->serialize((char*)&data[i]) : 0));
}

RankBitmap::BitmapPart::BitmapPart(char* p) {
  int* header = (int*)p;

  startBitNr = header[0];
  length = header[1];
  bits = (long*)malloc(sizeof(long) * length);

  long* data = (long*)(&header[3]);

  int i;
  for (i = 0; i < length; i++) {
    bits[i] = data[i];
  }

  if (header[2]) {
    next = new BitmapPart((char*)&data[i]);
  } else {
    next = 0;
  }
}

void RankBitmap::BitmapPart::merge(BitmapPart* other) {
  if (other == 0) {
    return;
  }

  // Check if the bitmap is after the next
  if ((next != 0) && (next->startBitNr <= other->startBitNr)) {
    next->merge(other);
    return;
  }

  // Check if the bitmap overlaps with the next
  if ((next != 0) && (next->startBitNr < other->getLastBit())) {
    // Swap the two and merge afterwards
    BitmapPart* temp = other->next;
    other->next = next->next;
    next->next = temp;

    temp = next;
    next = other;

    // Merge the two
    next->merge(temp);

    // Check if I should merge with the result
    tryMergeNext();

    return; 
  }

  // Check if the bit maps do not overlap
  if (getLastBit() < other->startBitNr) {
    // Insert the other after this part
    BitmapPart* oldNext = next;
    BitmapPart* othersOldNext = other->next;

    other->next = oldNext;
    next = other;

    // Check if the newly added part should merge forward
    other->tryMergeNext();

    // Check if I should merge with the newly added part
    tryMergeNext();

    // Process the remaining nodes to be merged
    merge(othersOldNext);
    return;
  }

  // The bitmaps overlap and must be merged into one
  int otherStartIndex = getArrIndex(other->startBitNr);
  int otherEndIndex   = getArrIndex(other->getLastBit());

  // Check if we must expand to fit the other
  if (otherEndIndex >= length) {
    expand(otherEndIndex + 1 - length);
  }

  // Merge the two maps by OR'ing the data
  for (int i = otherStartIndex, j = 0; i <= otherEndIndex; i++, j++) {
    bits[i] = bits[i] | other->bits[j];
  }

  // Move on to merge the next
  BitmapPart* nextMerger = other->next;

  delete other;

  merge(nextMerger);
}

RankBitmap::RankBitmap() {
  bitmap = new BitmapPart();
}

RankBitmap::~RankBitmap() {
  if (bitmap != 0) {
    delete bitmap;
  }
}

RankBitmap::RankBitmap(char *payload) {
  bitmap = new BitmapPart(payload);
}

void RankBitmap::addRank(int rank) {
  bitmap->setBit(rank);
}

bool RankBitmap::hasRank(int rank) {
  return bitmap->getBit(rank);
}

int RankBitmap::getSize() {
  return bitmap->listSize();
}

int RankBitmap::serialize(char *p) {
  return bitmap->serialize(p);
}

string RankBitmap::toString() {
  string res, desc;
  int count = bitmap->describe(desc);
  
  if (count == 0) {
    res = "0";
  } else {
    char buffer[64];
    snprintf(buffer, 64, "%d [", count);

    desc.resize(desc.size() - 1);

    res.append(buffer);
    res.append(desc);
    res.append("]");
  }

  return res;
}

void RankBitmap::merge(RankBitmap* other) {
  // This function merges two bitmaps into one.
  // The other bitmap will be empty afterwards.
  BitmapPart* merger;

  // The merge assumes that the current is lower or equal than the other bitmap.
  if (other->bitmap->startBitNr < bitmap->startBitNr) {
    merger = bitmap;
    bitmap = other->bitmap;
  } else {
    merger = other->bitmap;
  }

  bitmap->merge(merger);

  // Empty the other bitmap
  other->bitmap = new BitmapPart();
}
