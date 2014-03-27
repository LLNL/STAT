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
#include "DysectAPI.h"

using namespace std;
using namespace DysectAPI;

bool DescribeVariable::collect(void* process, void *thread) {
  return true;
}

bool DescribeVariable::fetchAggregates(Probe* probe) {
  if(!probe)
    return false;

  aggregates.clear();
  output.clear();
  outStr = "";

  int specId = strAgg->getId();

  AggregateFunction* specAgg = probe->getAggregate(specId);
  if(!specAgg) {
    fprintf(stderr, "Var spec aggregate not found\n");
    return false;
  }

  if(specAgg->getType() != staticStrAgg) {
    fprintf(stderr, "Var spec aggregate is of wrong type\n");
    return false;
  }

  StaticStr* staticSpecAgg = dynamic_cast<StaticStr*>(specAgg);

  map<string, int> countMap;
  staticSpecAgg->getCountMap(countMap);

  map<string, int>::iterator mapIter = countMap.begin();
  for(;mapIter != countMap.end(); mapIter++) {
    string spec = mapIter->first;

    enum parser_state {
      nameState,
      typeState,
      minidState,
      maxidState
    } state = nameState;

    string name, minidStr, maxidStr, type;
    int len = spec.size();
    const char* str = spec.c_str();
    for(int i = 0; i < len; i++) {
      char c = str[i];

      if((state == nameState) && (c == '[')) {
        state = typeState;
      } else if((state == typeState) && (c == ':')) {
        state = minidState;
      } else if((state == minidState) && (c == ':')) {
        state = maxidState;
      } else if((state == maxidState) && (c == ']')) {
        
        int minid = atoi(minidStr.c_str());
        int maxid = atoi(maxidStr.c_str());

        // Get min value
        string minStr;
        AggregateFunction* aggFunc = probe->getAggregate(minid);
        if(!aggFunc) {
          return Err::warn(false, "Could not get Min aggregate with id %d", minid);
        }

        if(aggFunc->getType() != minAgg) {
          return Err::warn(false, "Expected Min aggregate for id %d", minid);
        }

        Min* minInstance = dynamic_cast<Min*>(aggFunc);
        if(minInstance) {
          minInstance->getStr(minStr);
        }

        // Get max value
        string maxStr;
        aggFunc = probe->getAggregate(maxid);
        if(!aggFunc) {
          return Err::warn(false, "Could not get Max aggregate with id %d", maxid);
        }

        if(aggFunc->getType() != maxAgg) {
          return Err::warn(false, "Expected Min aggregate for id %d", maxid);
        }

        Max* maxInstance = dynamic_cast<Max*>(aggFunc);
        if(maxInstance) {
          maxInstance->getStr(maxStr);
        }

        const int bufSize = 512;
        char buf[bufSize];

        //XXX: Hack to print pointers
        if(type.compare("%p") == 0) {
          long minVal = atol(minStr.c_str());
          long maxVal = atol(maxStr.c_str());

          snprintf((char*)buf, bufSize, "%s = [%p:%p] ", name.c_str(), minVal, maxVal);
        } else {
          snprintf((char*)buf, bufSize, "%s = [%s:%s] ", name.c_str(), minStr.c_str(), maxStr.c_str());
        }

        Err::verbose(true, "Adding string: %s", buf);
        outStr.append(buf); 
  
        name = "";
        minidStr = "";
        maxidStr = "";
        type = "";

      } else {

        if(state == nameState) {
          name += c;
        }

        if(state == typeState) {
          type += c;
        }

        if(state == minidState) {
          minidStr += c;
        }

        if(state == maxidState) {
          maxidStr += c;
        }
      }
    }
  }

  return true;
}
