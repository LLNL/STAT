/*
Copyright (c) 2013-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Niklas Nielsen, Gregory Lee [lee218@llnl.gov], Dong Ahn.
LLNL-CODE-645136.
All rights reserved.

This file is part of DysectAPI. For details, see https://github.com/lee218llnl/DysectAPI. Please also read dysect/LICENSE

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA.
*/

#include <stdio.h>
#include <string.h>

#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI/Aggregates/CountInvocationsAgg.h"

using namespace std;
using namespace DysectAPI;

CountInvocationsAgg::CountInvocationsAgg(Probe *inOwner) : AggregateFunction(inOwner) {
  count_ = 1;
  type = countInvocationsAgg;
  counter = 0;
}

CountInvocationsAgg::CountInvocationsAgg() : AggregateFunction() {
  id_ = genId();
  count_ = 1;
  type = countInvocationsAgg;
  counter = 0;
}

CountInvocationsAgg::CountInvocationsAgg(int id, int count, std::string fmt, void* payload) {
  id_ = id;
  count_ = count;
  type = countInvocationsAgg;
  counter = 0;
  deserialize(payload);
}

void CountInvocationsAgg::copy(CountInvocationsAgg* other) {
  counter = other->counter;
}

bool CountInvocationsAgg::aggregate(AggregateFunction* agg) {
  if(agg->getType() != type)
    return false;

  if(agg->getId() != getId())
    return false;

  CountInvocationsAgg* countInvocationsInstance = dynamic_cast<CountInvocationsAgg*>(agg);

  if(!countInvocationsInstance) {
    return false;
  }

  counter += countInvocationsInstance->counter;
  count_ += agg->getCount();

  return true;
}

int CountInvocationsAgg::getCounter() {
  return counter;
}

bool CountInvocationsAgg::deserialize(void* payload) {
  // Deserialize
  char *ptr;

  ptr = (char *)payload;
  counter = *(int *)ptr;

  return true;
}

bool CountInvocationsAgg::readSubpacket(char* p) {
  struct subPacket* packet = (struct subPacket*)p;

  return deserialize(&packet->payload);
}

int CountInvocationsAgg::getSize() {
  //
  // Packet format
  //
  //  [ Header      ]
  //  [ int counter ]
  //

  return sizeof(struct subPacket) - sizeof(void*) + sizeof(int);
}

int CountInvocationsAgg::writeSubpacket(char *p) {
  struct subPacket* packet;

  packet = (struct subPacket*)p;
  packet->len = getSize();
  packet->id = getId();
  packet->count = count_;
  packet->type = type;
  memset(&(packet->fmt), 0, maxFmt); // Does not take format

  // Write
  char* curpos = (char*)&(packet->payload);

  memcpy(curpos, &counter, sizeof(int));

  return packet->len;
}

bool CountInvocationsAgg::clear() {
  counter = 0;
  count_ = 0;
}

bool CountInvocationsAgg::getStr(string& str) {
  const int bufSize = 512;
  char buf[bufSize];

  snprintf(buf, bufSize, "%d", counter);
  str.append(buf);

  return true;
}


bool CountInvocationsAgg::print() {
  string str;
  bool ret;

  ret = getStr(str);
  if (ret == false)
    return false;
  printf("Invocation count: %s\n", str.c_str());

  return true;
}

void CountInvocationsAgg::addValue(int value) {
  counter += value;
}

bool CountInvocationsAgg::collect(void* process, void *thread) {
  /* This aggregator is used by TraceAPI and does therefore
      not perform data collection itself. */

  return true;
}


bool CountInvocationsAgg::isSynthetic() {
  return false;
}

bool CountInvocationsAgg::getRealAggregates(vector<AggregateFunction*>& aggregates) {
  return true;
}


bool CountInvocationsAgg::fetchAggregates(Probe* probe) {
  return true;
}

