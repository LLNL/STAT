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

long AggregateFunction::counterId = 0;

bool AggregateFunction::getPacket(std::vector<AggregateFunction*>& aggregates, int& len, struct packet*& ptr) {
  
  
  std::vector<AggregateFunction*>::iterator aggIter;
  
  // Compute size of subpackets
  int subpacketsSize = 0;
  for(aggIter = aggregates.begin(); aggIter != aggregates.end(); aggIter++) {
    AggregateFunction* agg = *aggIter;
    subpacketsSize += agg->getSize();
  }
  
  if(subpacketsSize < 0) {
    return false;
  }


  int headerSize = sizeof(int);
  //printf(">> Packet header size %d (%d - %d)\n", headerSize, sizeof(struct packet), sizeof(void*));
  len = headerSize + subpacketsSize;
  ptr = (struct packet*)malloc(len);
  
  ptr->num = aggregates.size();

  char *payload = (char*)ptr + headerSize;
  
  if(!payload) {
    return false;
  }
  
  // Fetch subpackets and populate packetData
  char *curpos = payload;
  char *endpos = payload + subpacketsSize;
  for(aggIter = aggregates.begin(); aggIter != aggregates.end(); aggIter++) {
    AggregateFunction* agg = *aggIter;
    
    if(curpos >= endpos) {
      break;
    }
    
    curpos += agg->writeSubpacket(curpos);
  }

  //printf("## Packet len: %d\n", len);

  //char *p = (char*)ptr;
  //printf(">> Packet > hex contents: 0x");
  //for(int i = 0; i < len; i++) {
  //  printf("%02x", p[i]);
  //}

  //printf("\n>> Packet > char contents:");
  //for(int i = 0; i < len; i++) {
  //  printf("%c", p[i]);
  //}
  //printf("\n");

  return true;
}

bool AggregateFunction::mergePackets(struct packet* ptr1, struct packet* ptr2, struct packet*& ptr3, int& len) {
  std::map<int, AggregateFunction*> aggregates1;
  std::map<int, AggregateFunction*> aggregates2;
  
  getAggregates(aggregates1, ptr1);
  getAggregates(aggregates2, ptr2);
  
  
  std::map<int, AggregateFunction*>::iterator agg1iter = aggregates1.begin();
  std::map<int, AggregateFunction*>::iterator agg2iter;
  std::vector<AggregateFunction*> aggregates3;
  
  for(;agg1iter != aggregates1.end(); agg1iter++) {
    int id = agg1iter->first;
    AggregateFunction* agg1 = agg1iter->second;
    
    agg2iter = aggregates2.find(id);
    if(agg2iter != aggregates2.end()) {
      AggregateFunction* agg2 = agg2iter->second;
      
      agg1->aggregate(agg2);
      
      aggregates2.erase(agg2iter);
    }
    
    aggregates3.push_back(agg1);
  }
  
  // If any aggregates left in aggregates 2 - add them to new package
  agg2iter = aggregates2.begin();
  for(;agg2iter != aggregates2.end(); agg2iter++) {
    AggregateFunction* agg2 = agg2iter->second;
    aggregates3.push_back(agg2);
  }
  
  if(!AggregateFunction::getPacket(aggregates3, len, ptr3)) {
    std::cerr << "Aggregate packet could not be generated" << std::endl;
  }
  
  return true;
}

bool AggregateFunction::getAggregates(std::map<int, AggregateFunction*>& aggregates, struct packet* ptr) {
  if(!ptr)
    return false;

  int num = ptr->num;
  char *curpos = ((char*)ptr) + sizeof(int);
  
  for(int i = 0; i < num; i++) {
    AggregateFunction* aggFunc;
    curpos += getAggregate(curpos, aggFunc);

    if(!aggFunc) {
      fprintf(stderr, "Aggregate object could not be constructed from subpacket\n");
      return false;
    }
    
//    fprintf(stderr, "Adding aggregate %d", aggFunc->getId());

    aggregates.insert(std::pair<int, AggregateFunction*>(aggFunc->getId(), aggFunc));
  }
  
  return true;
}

int AggregateFunction::getAggregate(char *p, AggregateFunction*& aggFunc) {
  struct subPacket* ptr;
  
  ptr = (struct subPacket*)p;

  int plen = sizeof(struct subPacket);
  //printf(">>> Subpacket: 0x");
  //for(int i = 0; i < plen; i++) {
  //  printf("%02x", p[i]);
  //}
  //printf("\n");
  
  switch(ptr->type) {
    case minAgg:
      aggFunc = new Min(ptr->id, ptr->count, ptr->fmt, (void*)&(ptr->payload));
      break;
    case maxAgg:
      aggFunc = new Max(ptr->id, ptr->count, ptr->fmt, (void*)&(ptr->payload));
      break;
    case funcLocAgg:
      aggFunc = new FuncLocation(ptr->id, ptr->count, ptr->fmt, (void*)&(ptr->payload));
      break;
    case fileLocAgg:
      aggFunc = new FileLocation(ptr->id, ptr->count, ptr->fmt, (void*)&(ptr->payload));
      break;
    case paramNamesAgg:
      aggFunc = new FuncParamNames(ptr->id, ptr->count, ptr->fmt, (void*)&(ptr->payload));
      break;
    case staticStrAgg:
      aggFunc = new StaticStr(ptr->id, ptr->count, ptr->fmt, (void*)&(ptr->payload));
      break;
    case tracesAgg:
      aggFunc = new StackTraces(ptr->id, ptr->count, ptr->fmt, (void*)&(ptr->payload));
      break;
    case rankListAgg:
      aggFunc = new RankListAgg(ptr->id, ptr->count, ptr->fmt, (void*)&(ptr->payload));
      break;
    default:
      fprintf(stderr, "Unknown aggregate '%d'\n", ptr->type);
      aggFunc = 0;
      break;
  }
  
  return ptr->len;
}


bool Agg::aggregateIdFromName(std::string name, int& id) {
  bool found = false;

  if(name.compare("min") == 0) {
    id = minAgg;
    found = true;
  } else if(name.compare("max") == 0) {
    id = maxAgg;
    found = true;
  } else if(name.compare("function") == 0) {
    id = funcLocAgg;
    found = true;
  } else if(name.compare("location") == 0) {
    id = fileLocAgg;
    found = true;
  } else if(name.compare("params") == 0) {
    id = paramNamesAgg;
    found = true;
  } else if(name.compare("stackTraces") == 0) {
    id = tracesAgg;
    found = true;
  } else if(name.compare("desc") == 0) {
    id = descAgg;
    found = true;
  } else if(name.compare("ranks") == 0) {
    id = rankListAgg;
    found = true;
  }

  return found;
}
