#include "DysectAPI/Aggregates/Aggregate.h"

using namespace std;
using namespace DysectAPI;

bool FuncLocation::collect(void* process, void *thread) {
  return false;
}

bool FileLocation::collect(void* process, void *thread) {
  return false;
}

bool FuncParamNames::collect(void* process, void* thread) {
  return false;
}

bool StackTraces::collect(void* process, void* thread) {
  return false;
}
