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

#ifndef __DYSECTAPI_AGGREGATE_h
#define __DYSECTAPI_AGGREGATE_h

#include <string>

enum agg_type {
  minAgg = 1,
  maxAgg = 2,
  // Dummy aggregate
  firstAgg = 3,
  lastAgg = 4,
  timeListAgg = 5,
  bucketAgg = 6,
  rankBucketAgg = 7,
  // String aggregates
  funcLocAgg = 8,
  fileLocAgg = 9,
  paramNamesAgg = 10,
  tracesAgg = 11,
  staticStrAgg = 12,

  // Folds to aggregates
  descAgg = 13,

  rankListAgg = 14
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
#include "DysectAPI/Aggregates/StrAgg.h"
#include "DysectAPI/Aggregates/RankListAgg.h"
#include "DysectAPI/Aggregates/TimeListAgg.h"
#include "DysectAPI/Aggregates/BucketAgg.h"
#include "DysectAPI/Aggregates/RankBucketAgg.h"
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
