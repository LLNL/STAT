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
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdio.h>
#include <string.h>

#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI/Aggregates/AverageAgg.h"

using namespace std;
using namespace DysectAPI;

AverageAgg::AverageAgg(Probe *inOwner) : AggregateFunction(inOwner) {
  count_ = 1;
  type = averageAgg;
}

AverageAgg::AverageAgg() : AggregateFunction() {
  id_ = genId();
  count_ = 1;
  type = averageAgg;
}

AverageAgg::AverageAgg(int id, int count, std::string fmt, void* payload) {
  id_ = id;
  count_ = count;
  type = averageAgg;
  deserialize(payload);
}

void AverageAgg::copy(AverageAgg* other) {
  average = other->average;
  count = other->count;
}

bool AverageAgg::aggregate(AggregateFunction* agg) {
  if(agg->getType() != type)
    return false;
  
  if(agg->getId() != getId())
    return false;
  
  AverageAgg* averageInstance = dynamic_cast<AverageAgg*>(agg);
  
  if(!averageInstance) {
    return false;
  }

  addValue(averageInstance->average, averageInstance->count);
  averageInstance->clear();
  
  return true;
}

void AverageAgg::addValue(double otrAverage, double otrCount) {
  double myTotal = average * count;
  double otrTotal = otrAverage * otrCount;

  count += otrCount;
  average = (myTotal + otrTotal) / count;
}

double AverageAgg::getAverage() {
  return average;
}
double AverageAgg::getCount() {
  return count;
}


bool AverageAgg::deserialize(void* payload) {
  // Deserialize
  char *ptr = (char *)payload;

  average = *(double *)ptr;
  ptr += sizeof(double);

  count = *(double *)ptr;
  ptr += sizeof(double);

  return true;
}

bool AverageAgg::readSubpacket(char* p) {
  struct subPacket* packet = (struct subPacket*)p;

  return deserialize(&packet->payload);
}

int AverageAgg::getSize() {
  //
  // Packet format
  //  
  //  [ Header                   ]
  //  [ double average           ]
  //  [ double count             ]
  //

  return sizeof(struct subPacket) - sizeof(void*) + sizeof(double) * 2;
}

int AverageAgg::writeSubpacket(char *p) {
  struct subPacket* packet;
 
  packet = (struct subPacket*)p;
  packet->len = getSize();
  packet->id = getId();
  packet->count = count_;
  packet->type = type;
  memset(&(packet->fmt), 0, maxFmt); // Does not take format
 
  // Write 
  char* curpos = (char*)&(packet->payload);

  memcpy(curpos, &average, sizeof(double));
  curpos += sizeof(double);
  
  memcpy(curpos, &count, sizeof(double));
  curpos += sizeof(double);

  return packet->len;
}

bool AverageAgg::clear() {
  average = 0.0;
  count = 0.0;
}

bool AverageAgg::getStr(string& str) {
  const int bufSize = 512;
  char buf[bufSize];

  snprintf(buf, bufSize, "%f,", average);
  str.append(buf);

  return true;
}


bool AverageAgg::print() {
  string str;
  bool ret;
  
  ret = getStr(str);
  if (ret == false)
    return false;
  printf("average: %s\n", str.c_str());

  return true;
}

bool AverageAgg::collect(void* process, void *thread) {
  /* This aggregator is used by TraceAPI and does therefore
      not perform data collection itself. */

  return true;
}


bool AverageAgg::isSynthetic() {
  return false;
}

bool AverageAgg::getRealAggregates(vector<AggregateFunction*>& aggregates) {
  return true;
}


bool AverageAgg::fetchAggregates(Probe* probe) {
  return true;
}

