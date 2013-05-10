#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

bool DescribeVariable::collect(void* process, void *thread) {
  Process::const_ptr process_ptr = *(Process::const_ptr*)process;
  Thread::const_ptr thread_ptr = *(Thread::const_ptr*)thread;

  if(!process_ptr) {
    return Err::verbose(false, "Process object not available");
  }

  Walker* proc = (Walker*)process_ptr->getData();

  if(!proc) {
    return Err::verbose(false, "Could not get walker from process");
  }
  
  DataLocation* varLocation;

  if(!DataLocation::findVariable(process_ptr, proc, varName, varLocation)) {
    return Err::warn(false, "Could not locate variable '%s'", varName.c_str());
  }

  string varSpec = "";

  if(varLocation) {
    if(!varLocation->isStructure()) {
      
      // XXX: Add more sophisticated procedure to recognize
      // already prepared aggregates
      if(varSpecs.empty()) {
        const int bufSize = 512;
        char buf[bufSize];

        Min* minagg = new Min("%d", varName.c_str());
        Max* maxagg = new Max("%d", varName.c_str());

        if(!minagg) {
          return Err::verbose(false, "Minimum aggregate could not be created");
        }

        minagg->collect(process, thread);
        maxagg->collect(process, thread);
      
        aggregates.insert(pair<int, AggregateFunction*>(minagg->getId(), minagg));
        aggregates.insert(pair<int, AggregateFunction*>(maxagg->getId(), maxagg));

        string format = "%d";
        Type* symType = varLocation->getType();
        if(symType && symType->getPointerType()) {
          format="%p";
        } else {
          string typeName = symType->getName();
        }

        snprintf((char*)&buf, bufSize, "%s[%s:%d:%d]", varName.c_str(), format.c_str(), minagg->getId(), maxagg->getId());
        varSpec.append(buf);
  
        varSpecs.push_back(varSpec);

        Err::verbose(true, "Var spec: %s", varSpec.c_str());
      } else {
        // Collect existing
        if(!aggregates.empty()) {
          map<int, AggregateFunction*>::iterator aggIter = aggregates.begin();
          for(; aggIter != aggregates.end(); aggIter++) {
            AggregateFunction* aggFunc = aggIter->second;
            if(aggFunc) {
              aggFunc->collect(process, thread);
            }
          }
        }
      }
    } else {
      Err::verbose(true, "DescribeVariable(%s) collected: isStructure(%s)", varName.c_str(), varLocation->isStructure() ? "yes" : "no");
      Type* symType = varLocation->getType();
      typeStruct *stType = symType->getStructType();
      if(stType) {
        vector<Field*>* fields = stType->getComponents();
        if(fields) {
          vector<Field*>::iterator fieldIter = fields->begin();

          for(;fieldIter != fields->end(); fieldIter++) {
            Field* field = *fieldIter;

            if(field) {
              Err::verbose(true, "Member: '%s'", field->getName().c_str());
            }
          }
        }
      }
    }
  }

  

  return true;
}

bool DescribeVariable::fetchAggregates(Probe* probe) {
  return true;
}
