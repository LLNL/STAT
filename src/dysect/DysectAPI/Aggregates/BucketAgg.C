/*
Copyright (c) 2013-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Jesper Puge Nielsen, Niklas Nielsen, Gregory Lee [lee218@llnl.gov], Dong Ahn.
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

BucketAgg::BucketAgg(int id, int count, std::string fmt, void* payload) {
  variableType = Value::noType;
  type = bucketAgg;
  id_ = id;
  count_ = count;
  readSubpacket((char*)payload);
}

BucketAgg::BucketAgg(Probe* owner, string description) : AggregateFunction(owner) {
  variableType = Value::noType;
  type = bucketAgg;

  if (!parseDescription(description)) {
    DYSECTWARN(false, "Unknown bucket description: '%s'", description.c_str());
  }
}

bool BucketAgg::parseDescription(string description) {
  // The format of descritions are variableName,rangeStart:step:rangeEnd
  const char* desc = description.c_str();
  int tokenStart = 0;
  int curPos = 0;

  // Make sure the AggFunction is not already initialized
  if (buckets.size() != 0) {
    return false;
  }

  // Search for the comma
  while (desc[curPos] != ',') {
    if (desc[curPos] == 0) {
      // String ended too soon
      return false;
    }

    curPos += 1;
  }

  // An empty string is not acceptable
  if (curPos == 0) {
    return false;
  }

  // Copy the variable name
  variableName = description.substr(tokenStart, curPos - tokenStart);
  curPos += 1;

  // Parse the bucket range start
  if (!parseNumber(description, curPos, rangeStart) || desc[curPos] != ':') {
    return DYSECTWARN(false, "Invalid range start in '%s'", description.c_str());
  }
  curPos += 1;

  // Parse the bucket range start
  if (!parseNumber(description, curPos, stepSize) || desc[curPos] != ':') {
    return DYSECTWARN(false, "Invalid stepSize in '%s'", description.c_str());
  }
  curPos += 1;

  // Parse the bucket range start
  if (!parseNumber(description, curPos, rangeEnd) || desc[curPos] != 0) {
    return DYSECTWARN(false, "Invalid range end in '%s'", description.c_str());
  }

  // The types must match
  if ((rangeStart.getType() != rangeEnd.getType()) ||
      (rangeStart.getType() != stepSize.getType())) {
    return DYSECTWARN(false, "The range types does not match in '%s'", description.c_str());
  }

  // Calculate the number of buckets
  if (rangeEnd <= rangeStart) {
    return false;
  }

  if (rangeStart.getType() == Value::longType) {
    long rangeSize = rangeEnd.getValue<long>() - rangeStart.getValue<long>();
    bucketCount = (int)(rangeSize / stepSize.getValue<long>());

    if (rangeSize % stepSize.getValue<long>() != 0) {
      bucketCount += 1;
    }
  } else if (rangeStart.getType() == Value::doubleType) {
    double rangeSize = rangeEnd.getValue<double>() - rangeStart.getValue<double>();
    double intPart, fracPart = modf(rangeSize / stepSize.getValue<double>(), &intPart);

    bucketCount = (int)intPart + (fracPart == 0.0 ? 0 : 1);
  }
  
  // Prepare the buckets, add an extra for values out of range
  buckets.insert(buckets.end(), bucketCount + 2, 0);

  // Log the result
  string rangeStartStr, stepSizeStr, rangeEndStr;
  rangeStart.getStr(rangeStartStr);
  stepSize.getStr(stepSizeStr);
  rangeEnd.getStr(rangeEndStr);

  DYSECTVERBOSE(false, "The variable %s, will be placed in %d buckets by %s:%s:%s", 
       variableName.c_str(), bucketCount, rangeStartStr.c_str(), stepSizeStr.c_str(), rangeEndStr.c_str());

  return true;
}

bool BucketAgg::parseNumber(string& str, int& curPos, Value& val) {
  const char* chrs = str.c_str();
  int tokenStart = curPos;
  int dots = 0;

  do {
    if ((chrs[curPos] < '0' || chrs[curPos] > '9') && 
        (chrs[curPos] != '-' && curPos == 0)      &&
        (chrs[curPos] != '.' && curPos > 0)) {
      // Illegal string format
      return false;
    }

    if (chrs[curPos] == '.') {
      dots += 1;
    }

    curPos += 1;
  } while (chrs[curPos] != ':' && chrs[curPos] != 0);

  string token = str.substr(tokenStart, curPos - tokenStart);

  if (dots == 0) {
    Value res(atol(token.c_str()));

    val.copy(res);
  } else if (dots == 1) {
    Value res(atof(token.c_str()));

    val.copy(res);
  }
  
  return true;
}

string BucketAgg::generateDescription() {
  string rangeStartStr, stepSizeStr, rangeEndStr;
  string desc(variableName);

  rangeStart.getStr(rangeStartStr);
  stepSize.getStr(stepSizeStr);
  rangeEnd.getStr(rangeEndStr);

  desc.append(",");
  desc.append(rangeStartStr);
  desc.append(":");
  desc.append(stepSizeStr);
  desc.append(":");
  desc.append(rangeEndStr);

  return desc;
}

vector<int>& BucketAgg::getBuckets() {
  return buckets;
}

bool BucketAgg::aggregate(AggregateFunction* agg) {
  if(agg->getType() != type)
    return false;
  
  if(agg->getId() != getId())
    return false;
  
  BucketAgg* bucketInstance = dynamic_cast<BucketAgg*>(agg);
  
  if(!bucketInstance) {
    return false;
  }

  if (bucketInstance->bucketCount != bucketCount) {
    return false;
  }

  for (int i = 0; i <= bucketCount + 1; i++) {
    buckets[i] += bucketInstance->buckets[i];
  }

  return true;
}

int BucketAgg::getSize() {
  // 
  // Packet format
  //
  // [ header                       ]
  // [ int dataType                 ]
  // [ long rangeStart              ]
  // [ long stepSize                ]
  // [ long rangeEnd                ]
  // [ int bucketCount              ]
  // [ int[bucketCount + 2] buckets ]
  //

  return sizeof(struct subPacket) - sizeof(void*) + (sizeof(double) * 3) + (sizeof(int) * (bucketCount + 2 + 2));
}

int BucketAgg::writeSubpacket(char* p) {
  struct subPacket* packet;
 
  packet = (struct subPacket*)p;
  packet->len = getSize();
  packet->id = getId();
  packet->count = count_;
  packet->type = type;
  memset(&(packet->fmt), 0, maxFmt);

  // Write package content
  int* curpos = (int *)&(packet->payload);
  double* curposd = (double*)&curpos[1];
  long* curposl = (long*)&curpos[1];

  if (rangeStart.isLongLike()) {
    curpos[0] = 1;

    curposl[0] = rangeStart.asLong();
    curposl[1] = stepSize.asLong();
    curposl[2] = rangeEnd.asLong();
  } else {
    curpos[0] = 0;

    curposd[0] = rangeStart.asDouble();
    curposd[1] = stepSize.asDouble();
    curposd[2] = rangeEnd.asDouble();
  }
  
  curpos = (int*)&curposd[3];

  *curpos = bucketCount;
  curpos += 1;

  for (int i = 0; i <= bucketCount + 1; i++) {
    *curpos = buckets[i];
    curpos += 1;
  }

  return getSize();
}

bool BucketAgg::readSubpacket(char* payload) {
  int* curpos = (int*)payload;
  long* curposl = (long*)&curpos[1];
  double* curposd = (double*)&curpos[1];

  // Make sure the AggFunction is not already initialized
  if (buckets.size() != 0) {
    return false;
  }

  // Read the bucket dimensions
  if (*curpos == 1) {
    rangeStart.populate<long>(curposl[0]);
    rangeStart.setType(Value::longType);

    stepSize.populate<long>(curposl[1]);
    stepSize.setType(Value::longType);

    rangeEnd.populate<long>(curposl[2]);
    rangeEnd.setType(Value::longType);
  } else {
    rangeStart.populate<double>(curposd[0]);
    rangeStart.setType(Value::doubleType);

    stepSize.populate<double>(curposd[1]);
    stepSize.setType(Value::doubleType);

    rangeEnd.populate<double>(curposd[2]);
    rangeEnd.setType(Value::doubleType);
  }

  curpos = (int*)&curposd[3];

  bucketCount = *curpos;
  curpos += 1;

  // Read the bucket values
  buckets.insert(buckets.end(), curpos, curpos + bucketCount + 2);

  return true;
}

bool BucketAgg::clear() {
  count_ = 0;
  buckets.clear();
}

bool BucketAgg::getStr(string& str) {
  string curRangeStartStr, nextRangeStartStr;
  const int bufSize = 64;
  char buffer[bufSize];

  snprintf(buffer, bufSize, "Below bucket range: %d\n", buckets[bucketCount]);
  str.append(buffer);

  Value curRangeStart(rangeStart);
  for (int i = 0; i < bucketCount; i++) {
    Value nextRangeStart(curRangeStart + stepSize);
    nextRangeStart = (nextRangeStart > rangeEnd ? rangeEnd : nextRangeStart);

    curRangeStart.getStr(curRangeStartStr);
    nextRangeStart.getStr(nextRangeStartStr);

    snprintf(buffer, bufSize, " [%s,%s): %d\n", curRangeStartStr.c_str(), nextRangeStartStr.c_str(), buckets[i]);
    str.append(buffer);

    curRangeStart = nextRangeStart;
  }

  snprintf(buffer, bufSize, "Above bucket range: %d", buckets[bucketCount + 1]);
  str.append(buffer);

  return true;
}

bool BucketAgg::print() {
  string str;

  if (!getStr(str)) {
    return false;
  }

  printf("buckets: %s\n", str.c_str());

  return true;
}

int BucketAgg::getBucketFromValue(Value& val) {
  if (val < rangeStart) {
    return bucketCount;
  }

  if (val >= rangeEnd) {
    return bucketCount + 1;
  }

  if (val.isLongLike()) {
    long value = val.asLong();
    long lRangeStart = rangeStart.asLong();
    long lStepSize   = stepSize.asLong();

    return (int)((value - lRangeStart) / lStepSize);

  } else if (val.isDoubleLike()) {
    double value = val.asDouble();
    double dRangeStart = rangeStart.asDouble();
    double dStepSize   = stepSize.asDouble();

    return (int)((value - dRangeStart) / dStepSize);

  } else {
    DYSECTWARN("Invalid value type cannot be placed in bucket");

    return 0;
  }
}


bool BucketAgg::isSynthetic() {
  return false;
}

bool BucketAgg::getRealAggregates(vector<AggregateFunction*>& aggregates) {
  return true;
}

bool BucketAgg::fetchAggregates(Probe* probe) {
  return true;
}

