#include "DysectAPI/Aggregates/Aggregate.h"

using namespace std;
using namespace DysectAPI;

bool DescribeVariable::collect(void* process, void *thread) {
  return true;
}

bool DescribeVariable::fetchAggregates(Probe* probe) {
  return true;
}
