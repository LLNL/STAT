#ifndef __DYSECTAPI_AGGREGATE_h
#define __DYSECTAPI_AGGREGATE_h

enum agg_type {
  minAgg = 1,
  maxAgg = 2,
  // String aggregates
  funcLocAgg = 3,
  fileLocAgg = 4,
  paramNamesAgg = 5,
  tracesAgg = 6,
  staticStrAgg = 7,

  // Folds to aggregates
  descAgg = 8
};

#include <typeinfo>
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "DysectAPI/Aggregates/Data.h"
#include "DysectAPI/Aggregates/AggregateFunction.h"
#include "DysectAPI/Aggregates/CmpAgg.h"
//#include "DysectAPI/Aggregates/Min.h"
#include "DysectAPI/Aggregates/StrAgg.h"
#include "DysectAPI/Aggregates/Location.h"
#include "DysectAPI/Aggregates/DescVar.h"
#include "DysectAPI/Aggregates/Packet.h"

namespace DysectAPI {
  class Agg {
  public:
    static bool aggregateIdFromName(std::string name, int& id);
  };
}

#endif
