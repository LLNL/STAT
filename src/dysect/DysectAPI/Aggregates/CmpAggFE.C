#include "DysectAPI/Aggregates/Aggregate.h"

using namespace std;
using namespace DysectAPI;

bool CmpAgg::collect(void *process, void *thread) {
  return true;
}

Min::Min(std::string fmt, ...) : CmpAgg(minAgg, fmt) {
}

Max::Max(std::string fmt, ...) : CmpAgg(maxAgg, fmt) {
}
