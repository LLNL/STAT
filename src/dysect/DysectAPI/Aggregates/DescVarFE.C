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
