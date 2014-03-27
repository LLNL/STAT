#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI.h"

using namespace std;
using namespace DysectAPI;

bool AggregateFunction::getParams(std::string fmt, std::vector<DataRef*>& params, va_list args) {
  const char* fmtStr = fmt.c_str();
  int len = fmt.size();
  bool next = false;
  char c;
  
  for(int i = 0; i < len; i++) {
    c = fmtStr[i];
    
    if(next) {
      switch(c) {
        case 'd': {
          const char* name = va_arg(args, const char*);
          params.push_back(new DataRef(Value::intType, name));
          next = false;
          break;
        }  
        case 'l': {
          const char* name = va_arg(args, const char*);
          params.push_back(new DataRef(Value::longType, name));
          next = false;
          break;
        }
        default:
          fprintf(stderr, "Unknown format specifier '%c'\n", c);
          return false;
          break;
      }
    } else if(c == '%') {
      next = true;
    }
  }
  
  return true;
}
