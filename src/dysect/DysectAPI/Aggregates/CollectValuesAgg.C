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
#include "DysectAPI/Aggregates/CollectValuesAgg.h"

using namespace std;
using namespace DysectAPI;

CollectValuesAgg::CollectValuesAgg(Probe *inOwner) : AggregateFunction(inOwner) {
  count_ = 1;
  type = collectValuesAgg;
}

CollectValuesAgg::CollectValuesAgg() : AggregateFunction() {
  id_ = genId();
  count_ = 1;
  type = collectValuesAgg;
}

CollectValuesAgg::CollectValuesAgg(int id, int count, std::string fmt, void* payload) {
  id_ = id;
  count_ = count;
  type = collectValuesAgg;
  deserialize(payload);
}

void CollectValuesAgg::copy(CollectValuesAgg* other) {
  values = other->values;
}

bool CollectValuesAgg::aggregate(AggregateFunction* agg) {
  if(agg->getType() != type)
    return false;

  if(agg->getId() != getId())
    return false;

  CollectValuesAgg* collectValuesInstance = dynamic_cast<CollectValuesAgg*>(agg);

  if(!collectValuesInstance) {
    return false;
  }

  set<int>& otherValues = collectValuesInstance->getValues();
  values.insert(otherValues.begin(), otherValues.end());
  otherValues.clear();
  count_ += agg->getCount();

  return true;
}

std::set<int>& CollectValuesAgg::getValues() {
  return values;
}

bool CollectValuesAgg::deserialize(void* payload) {
  // Deserialize
  char *ptr;
  int i;
  int numInts;

  ptr = (char *)payload;
  numInts = *(int *)ptr;
  ptr += sizeof(int);

  for (i = 0; i < numInts; i++) {
    int curNum = *(int *)ptr;
    ptr += sizeof(int);
    addValue(curNum);
  }

  return true;
}

bool CollectValuesAgg::readSubpacket(char* p) {
  struct subPacket* packet = (struct subPacket*)p;

  return deserialize(&packet->payload);
}

int CollectValuesAgg::getSize() {
  //
  // Packet format
  //
  //  [ Header                   ]
  //  [ int numInts              ]
  //  [ int[numInts] values      ]
  //

  return sizeof(struct subPacket) - sizeof(void*) + sizeof(int) + (values.size() * sizeof(int));
}

int CollectValuesAgg::writeSubpacket(char *p) {
  struct subPacket* packet;

  packet = (struct subPacket*)p;
  packet->len = getSize();
  packet->id = getId();
  packet->count = count_;
  packet->type = type;
  memset(&(packet->fmt), 0, maxFmt); // Does not take format

  // Write
  char* curpos = (char*)&(packet->payload);

  int numInts = values.size();
  memcpy(curpos, &numInts, sizeof(int));

  curpos += sizeof(int);

  set<int>::iterator it;
  for(it = values.begin(); it != values.end(); ++it) {
    int curNum = *it;
    memcpy(curpos, &curNum, sizeof(int));
    curpos += sizeof(int);
  }

  return packet->len;
}

bool CollectValuesAgg::clear() {
  count_ = 0;
  count_ = 0;
  values.clear();
}

bool CollectValuesAgg::getStr(string& str) {
  const int bufSize = 512;
  char buf[bufSize];

  set<int>::iterator it;
  for(it = values.begin(); it != values.end(); ++it) {
    snprintf(buf, bufSize, "%d,", *it);
    str.append(buf);
  }

  return true;
}


bool CollectValuesAgg::print() {
  string str;
  bool ret;

  ret = getStr(str);
  if (ret == false)
    return false;
  printf("collected values: %s\n", str.c_str());

  return true;
}

void CollectValuesAgg::addValue(int value) {
  values.insert(value);
}

bool CollectValuesAgg::collect(void* process, void *thread) {
  /* This aggregator is used by TraceAPI and does therefore
      not perform data collection itself. */

  return true;
}

void CollectValuesAgg::getValues(std::set<int>& outValues) {
  outValues = this->values;
}


bool CollectValuesAgg::isSynthetic() {
  return false;
}

bool CollectValuesAgg::getRealAggregates(vector<AggregateFunction*>& aggregates) {
  return true;
}


bool CollectValuesAgg::fetchAggregates(Probe* probe) {
  return true;
}

