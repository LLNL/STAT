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
#include "DysectAPI/Aggregates/RankListAgg.h"

using namespace std;
using namespace DysectAPI;

RankListAgg::RankListAgg(Probe *inOwner) : AggregateFunction(inOwner) {
  count_ = 1;
  type = rankListAgg;
}

RankListAgg::RankListAgg() : AggregateFunction() {
  count_ = 1;
  type = rankListAgg;
}

RankListAgg::RankListAgg(int id, int count, std::string fmt, void* payload) {
  id_ = id;
  count_ = count;
  type = rankListAgg;
  deserialize(payload);
}

bool RankListAgg::aggregate(AggregateFunction* agg) {
  if(agg->getType() != type)
    return false;

  if(agg->getId() != getId())
    return false;

  RankListAgg* rankListInstance = dynamic_cast<RankListAgg*>(agg);

  if(!rankListInstance) {
    return false;
  }

  vector<int>& aggRankList = rankListInstance->getRankList();
  rankList.insert(rankList.end(), aggRankList.begin(), aggRankList.end());
  aggRankList.clear();
  count_ += agg->getCount();

  return true;
}

std::vector<int>& RankListAgg::getRankList() {
  return rankList;
}

bool RankListAgg::deserialize(void* payload) {
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
    rankList.push_back(curNum);
  }

  return true;
}

int RankListAgg::getSize() {
  //
  // Packet format
  //
  //  [ Header                   ]
  //  [ int numInts              ]
  //  [ int[numInts] rankList     ]
  //

  return sizeof(struct subPacket) - sizeof(void*) + sizeof(int) + (rankList.size() * sizeof(int));
}

int RankListAgg::writeSubpacket(char *p) {
  struct subPacket* packet;

  packet = (struct subPacket*)p;
  packet->len = getSize();
  packet->id = getId();
  packet->count = count_;
  packet->type = type;
  memset(&(packet->fmt), 0, maxFmt); // Does not take format

  // Write
  char* curpos = (char*)&(packet->payload);

  int numInts = rankList.size();
  memcpy(curpos, &numInts, sizeof(int));

  curpos += sizeof(int);

  int i;
  for(i = 0; i < rankList.size(); i++) {
    int curNum = rankList[i];
    memcpy(curpos, &curNum, sizeof(int));
    curpos += sizeof(int);
  }

  return getSize();
}

bool RankListAgg::clear() {
  count_ = 0;
  count_ = 0;
  rankList.clear();
}

bool RankListAgg::getStr(string& str) {
  const int bufSize = 512;
  char buf[bufSize];
  int i;

  for (i = 0; i < rankList.size(); i++) {
    snprintf(buf, bufSize, "%d,", rankList[i]);
    str.append(buf);
  }

  return true;
}


bool RankListAgg::print() {
  string str;
  bool ret;

  ret = getStr(str);
  if (ret == false)
    return false;
  printf("rank list: %s\n", str.c_str());

  return true;
}

void RankListAgg::getRankList(std::vector<int>& outRankList) {
  outRankList = this->rankList;
}


bool RankListAgg::isSynthetic() {
  return false;
}

bool RankListAgg::getRealAggregates(vector<AggregateFunction*>& aggregates) {
  return true;
}


bool RankListAgg::fetchAggregates(Probe* probe) {
  return true;
}

