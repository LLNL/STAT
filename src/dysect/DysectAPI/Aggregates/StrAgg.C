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
#include "DysectAPI/Aggregates/StrAgg.h"

using namespace std;
using namespace DysectAPI;

StrAgg::StrAgg(agg_type ltype) : AggregateFunction(ltype) {
  strsLen = 0;
  count_ = 1;
}

StrAgg::StrAgg(agg_type ltype, int id, int count, string fmt, void* payload) {
  id_ = id;
  count_ = count;
  type = ltype;
  strsLen = 0;
  deserialize(payload);
}

bool StrAgg::aggregate(AggregateFunction* agg) {
  if(agg->getType() != type)
    return false;
  
  if(agg->getId() != getId())
    return false;
  
  StrAgg* strInstance = dynamic_cast<StrAgg*>(agg);
  
  if(!strInstance) {
    return false;
  }

  map<string, int> localCountMap = getCountMap();
  map<string, int>& aggCountMap = strInstance->getCountMap();

  map<string, int>::iterator localMapIter = localCountMap.begin();
  map<string, int>::iterator aggMapIter;
  
  countMap.clear();

  int totalStrLength = 0;

  for(;localMapIter != localCountMap.end(); localMapIter++) {
    string localStr = localMapIter->first;
    int localCount = localMapIter->second;
    totalStrLength += localStr.size() + 1; // Include NULL-termination

    int aggCount = 0;

    aggMapIter = aggCountMap.find(localStr);
    if(aggMapIter != aggCountMap.end()) {
      aggCount = aggMapIter->second;
    }

    int resultCount = localCount + aggCount;

    countMap.insert(pair<string, int>(localStr, resultCount));

    aggCountMap.erase(aggMapIter);
  }

  // Take care of remaining names in aggFunc
  if(!aggCountMap.empty()) {
    aggMapIter = aggCountMap.begin();
    for(;aggMapIter != aggCountMap.end(); aggMapIter++) {
      string str = aggMapIter->first;
      totalStrLength += str.size() + 1;// Include NULL-termination

      int count = aggMapIter->second;

      countMap.insert(pair<string, int>(str, count));
    }
  }

  strsLen = totalStrLength;

  count_ += agg->getCount();
  
  return true;
}

std::map<string, int>& StrAgg::getCountMap() {
  return countMap;
}

bool StrAgg::deserialize(void* payload) {
  // Deserialize
  int num = *(int*)payload; // First bytes are for number of functions in payload

  //printf(">>> num %d\n", num);

  // Compute offsets
  int *counts = (int*)((int*)payload + sizeof(int));
  char* strs = (char*)((char*)payload + sizeof(int) + (num * sizeof(int)));

  char *ptr = strs;
  char *curStr = ptr;
  int curStrSize = 0;
  char c;
  int count = 0;
  int cur_num = 0;

  //while((ptr <= payloadEnd) && (cur_num <= num)) {
  while(cur_num < num) {
    c = *ptr;
    if(c == '\0') {
      if(curStrSize > 0) {
        //if((char*)(counts + (sizeof(int) * cur_num)) > funcNames) {
        //  fprintf(stderr, "Overrun of payload: count array overlaps with function names\n");
        //  return false;
        //}
        count = counts[cur_num++];
        countMap.insert(pair<string, int>(string(curStr), count));
        curStr = ptr + 1;
      }
    } else {
      curStrSize++;
    }

    ptr++;
  }

  return true;
}

int StrAgg::getSize() {
  //
  // Packet format
  //  
  //  [ Header                   ]
  //  [ int numFuncs             ]
  //  [ int[numFuncs] countArray ]
  //  [ char* functionNames      ]
  //
  int nameLen = 0;
  map<string, int>::iterator countMapIter = countMap.begin();
  for(; countMapIter != countMap.end(); countMapIter++) {
    string name = countMapIter->first;
    nameLen += name.size() + 1;
  }

  //printf(">> sizeof header: %d\n", sizeof(struct subPacket) - sizeof(void*));
  //printf(">> sizeof maxFmt: %d\n", maxFmt);
  //printf(">> sizeof numFuncs: %d\n", sizeof(int));
  //printf(">> sizeof countArr: %d\n", countMap.size() * sizeof(int));
  //printf(">> sizeof nameLen: %d\n", nameLen);

  return sizeof(struct subPacket) - sizeof(void*) + sizeof(int) + (countMap.size() * sizeof(int)) + nameLen;
}

int StrAgg::writeSubpacket(char *p) {
  struct subPacket* packet;
 
  //printf(">> Start address: 0x%08lx\n", (long)p);

  packet = (struct subPacket*)p;
  packet->len = getSize();
  packet->id = getId();
  packet->count = count_;
  packet->type = type;
  memset(&(packet->fmt), 0, maxFmt); // Does not take format
 
  // Write 
  char* curpos = (char*)&(packet->payload);

  //printf(">> Payload address: 0x%08lx\n", (long)curpos);

  int numStrs = countMap.size();
  memcpy(curpos, &numStrs, sizeof(int));

  //printf(">> numFuncs %d\n", numFuncs);

  curpos += sizeof(int);

  map<string, int>::iterator countMapIter = countMap.begin();
  for(; countMapIter != countMap.end(); countMapIter++) {
    int count = countMapIter->second;

    //printf(">> count %d\n", count);

    memcpy(curpos, &count, sizeof(int));
    curpos += sizeof(int);
  }

  countMapIter = countMap.begin();
  for(; countMapIter != countMap.end(); countMapIter++) {
    string curStr = countMapIter->first;

    int len = curStr.size() + 1;
    const char* cstr = curStr.c_str();

    //printf(">> str(%s)\n", cstr);

    strncpy(curpos, cstr, len);

    curpos += len;
  }

  int size = getSize();

  //printf(">> Constructed subpacket (%d)\n", size);

  //printf(">> Constructed subpacket > hex contents: 0x");
  //for(int i = 0; i < size; i++) {
  //  printf("%02x", p[i]);
  //}

  //printf("\n>> Constructed subpacket > char contents:");
  //for(int i = 0; i < size; i++) {
  //  printf("%c", p[i]);
  //}
  //printf("\n");

  
  return size;
}

bool StrAgg::clear() {
  count_ = 0;
  strsLen = 0;
  count_ = 0;
  countMap.clear();
}

bool StrAgg::getStr(string& str) {
  const int bufSize = 512;
  char buf[bufSize];

  map<string, int>::iterator countMapIter;
  
  if(countMap.size() == 1) {
    countMapIter = countMap.begin();
    string name = countMapIter->first;
    str.append(name);

  } else if(countMap.size() >= 1) {

    str.append("[ ");

    countMapIter = countMap.begin();
    for(; countMapIter != countMap.end(); countMapIter++) {
      int count = countMapIter->second;
      string name = countMapIter->first;

      sprintf((char*)&buf, "%d:%s ", count, name.c_str());

      str.append(buf);
    }

    str.append("]");
  }
  return true;
}


void StrAgg::getCountMap(std::map<std::string, int>& strCountMap) {
  strCountMap = this->countMap;
}


bool StrAgg::isSynthetic() {
  return false;
}

bool StrAgg::getRealAggregates(vector<AggregateFunction*>& aggregates) {
  return true;
}


bool StrAgg::fetchAggregates(Probe* probe) {
  return true;
}

