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

#include "DysectAPI/Aggregates/Aggregate.h"

using namespace std;
using namespace DysectAPI;

TimeListAgg::TimeListAgg(Probe *inOwner) : AggregateFunction(inOwner) {
  count_ = 1;
  type = timeListAgg;
}

TimeListAgg::TimeListAgg() : AggregateFunction() {
  count_ = 1;
  type = timeListAgg;
}

TimeListAgg::TimeListAgg(int id, int count, std::string fmt, void* payload) {
  id_ = id;
  count_ = count;
  type = timeListAgg;
  deserialize(payload);
}

std::vector<long>& TimeListAgg::getTimeList() {
  return timeList;
}

bool TimeListAgg::aggregate(AggregateFunction* agg) {
  if(agg->getType() != type)
    return false;

  if(agg->getId() != getId())
    return false;

  TimeListAgg* timeListInstance = dynamic_cast<TimeListAgg*>(agg);

  if(!timeListInstance) {
    return false;
  }

  vector<long>& aggList = timeListInstance->getTimeList();
  timeList.insert(timeList.end(), aggList.begin(), aggList.end());
  aggList.clear();
  count_ += agg->getCount();

  return true;
}

int TimeListAgg::getSize() {
  //
  // Packet format
  //
  //  [ Header                   ]
  //  [ int numInts              ]
  //  [ int[numInts] timeList    ]
  //

  return sizeof(struct subPacket) - sizeof(void*) + sizeof(int) + (timeList.size() * sizeof(long));
}

int TimeListAgg::writeSubpacket(char *p) {
  struct subPacket* packet;

  packet = (struct subPacket*)p;
  packet->len = getSize();
  packet->id = getId();
  packet->count = count_;
  packet->type = type;
  memset(&(packet->fmt), 0, maxFmt); // Does not take format

  // Write
  char* curpos = (char*)&(packet->payload);

  int numInts = timeList.size();
  memcpy(curpos, &numInts, sizeof(int));

  curpos += sizeof(int);

  int i;
  for(i = 0; i < timeList.size(); i++) {
    long curNum = timeList[i];
    memcpy(curpos, &curNum, sizeof(long));
    curpos += sizeof(long);
  }

  return getSize();
}

bool TimeListAgg::deserialize(void* payload) {
  // Deserialize
  char *ptr;
  int i;
  int numInts;

  ptr = (char *)payload;
  numInts = *(int *)ptr;
  ptr += sizeof(int);

  for (i = 0; i < numInts; i++) {
    long curNum = *(long *)ptr;
    ptr += sizeof(long);
    timeList.push_back(curNum);
  }

  return true;
}

bool TimeListAgg::clear() {
  count_ = 0;
  count_ = 0;
  timeList.clear();
}

bool TimeListAgg::getStr(string& str) {
  const int bufSize = 512;
  char buf[bufSize];
  int i;

  for (i = 0; i < timeList.size(); i++) {
    snprintf(buf, bufSize, "%d,", timeList[i]);
    str.append(buf);
  }

  return true;
}

bool TimeListAgg::print() {
  string str;
  bool ret;

  ret = getStr(str);
  if (ret == false)
    return false;
  printf("time list: %s\n", str.c_str());

  return true;
}

bool TimeListAgg::isSynthetic() {
  return false;
}

bool TimeListAgg::getRealAggregates(vector<AggregateFunction*>& aggregates) {
  return true;
}


bool TimeListAgg::fetchAggregates(Probe* probe) {
  return true;
}
