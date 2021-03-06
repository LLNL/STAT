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
#include <cmath>

using namespace std;
using namespace DysectAPI;

RankBucketAgg::RankBucketAgg(int id, int count, std::string fmt, void* payload) {
  variableType = Value::noType;
  type = rankBucketAgg;
  id_ = id;
  count_ = count;
  deserialize((char*)payload);
}

RankBucketAgg::RankBucketAgg(Probe* owner, string description) : AggregateFunction(owner) {
  variableType = Value::noType;
  type = rankBucketAgg;

  if (!parseDescription(description)) {

  }
}

RankBucketAgg::RankBucketAgg(int start, int end, int step, int count) : AggregateFunction() {
  id_ = genId();

  variableType = Value::longType;
  type = rankBucketAgg;

  rangeStart.copy(Value(start));
  rangeEnd.copy(Value(end));
  stepSize.copy(Value(step));
  bucketCount = count;

  // Prepare the buckets, add an extra for values out of range
  buckets.resize(bucketCount + 2);
  for (int i = 0; i <= bucketCount + 1; i++) {
    buckets[i] = new RankBitmap();
  }
}

RankBucketAgg::RankBucketAgg() {
  type = rankBucketAgg;
  id_ = genId();
}

RankBucketAgg::~RankBucketAgg() {
  for (int i = 0; i < bucketCount + 1; i++) {
    delete buckets[i];
  }
}

void RankBucketAgg::copy(RankBucketAgg* agg) {
  buckets = agg->buckets;

  variableName = agg->variableName;
  variableType = agg->variableType;

  rangeStart = agg->rangeStart;
  rangeEnd = agg->rangeEnd;
  stepSize = agg->stepSize;
  bucketCount = agg->bucketCount;
}

bool RankBucketAgg::parseDescription(string description) {
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
    return false;
  }
  curPos += 1;

  // Parse the bucket range start
  if (!parseNumber(description, curPos, stepSize) || desc[curPos] != ':') {
    return false;
  }
  curPos += 1;

  // Parse the bucket range start
  if (!parseNumber(description, curPos, rangeEnd) || desc[curPos] != 0) {
    return false;
  }

  // The types must match
  if ((rangeStart.getType() != rangeEnd.getType()) ||
      (rangeStart.getType() != stepSize.getType())) {
    return false;
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
  buckets.resize(bucketCount + 2);
  for (int i = 0; i <= bucketCount + 1; i++) {
    buckets[i] = new RankBitmap();
  }

  // Log the result
  string rangeStartStr, stepSizeStr, rangeEndStr;
  rangeStart.getStr(rangeStartStr);
  stepSize.getStr(stepSizeStr);
  rangeEnd.getStr(rangeEndStr);

  return true;
}

bool RankBucketAgg::parseNumber(string& str, int& curPos, Value& val) {
  const char* chrs = str.c_str();
  int tokenStart = curPos;
  int dots = 0;

  do {
    if ((chrs[curPos] < '0' || chrs[curPos] > '9') &&
        (chrs[curPos] != '-' && curPos == 0)       &&
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

string RankBucketAgg::generateDescription() {
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

vector<RankBitmap*>& RankBucketAgg::getBuckets() {
  return buckets;
}

bool RankBucketAgg::aggregate(AggregateFunction* agg) {
  if(agg->getType() != type)
    return false;

  if(agg->getId() != getId())
    return false;

  RankBucketAgg* bucketInstance = dynamic_cast<RankBucketAgg*>(agg);

  if(!bucketInstance) {
    return false;
  }

  if (bucketInstance->bucketCount != bucketCount) {
    return false;
  }

  for (int i = 0; i <= bucketCount + 1; i++) {
    buckets[i]->merge(bucketInstance->buckets[i]);
  }

  return true;
}

int RankBucketAgg::getSize() {
  //
  // Packet format
  //
  // [ header                              ]
  // [ int dataType                        ]
  // [ double rangeStart                   ]
  // [ double stepSize                     ]
  // [ double rangeEnd                     ]
  // [ int bucketCount                     ]
  // [ RankBitmap[bucketCount + 2] buckets ]
  //

  int size = sizeof(struct subPacket) - sizeof(void*) + (sizeof(double) * 3) + (sizeof(int) * 2);

  for (int i = 0; i <= bucketCount + 1; i++) {
    size += buckets[i]->getSize();
  }

  return size;
}

int RankBucketAgg::writeSubpacket(char* p) {
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


  char* bufPos = (char*)curpos;

  for (int i = 0; i <= bucketCount + 1; i++) {
    bufPos += buckets[i]->serialize(bufPos);
  }

  return bufPos - p;
}

bool RankBucketAgg::readSubpacket(char* p) {
  struct subPacket* packet = (struct subPacket*)p;

  return deserialize(&packet->payload);
}

bool RankBucketAgg::deserialize(void* payload) {
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
  char* bufPos = (char*)curpos;

  buckets.resize(bucketCount + 2);
  for (int i = 0; i <= bucketCount + 1; i++) {
    buckets[i] = new RankBitmap(bufPos);
    bufPos += buckets[i]->getSize();
  }

  return true;
}

bool RankBucketAgg::clear() {
  count_ = 0;

  for (int i = 0; i <= bucketCount + 1; i++) {
    delete buckets[i];
    buckets[i] = new RankBitmap();
  }
}

bool RankBucketAgg::getStr(string& str) {
  string curRangeStartStr, nextRangeStartStr;
  const int bufSize = 256;
  char buffer[bufSize];

  snprintf(buffer, bufSize, "Below bucket range: %s\n", buckets[bucketCount]->toString().c_str());

  str.append(buffer);

  Value curRangeStart(rangeStart);
  for (int i = 0; i < bucketCount; i++) {
    Value nextRangeStart(curRangeStart + stepSize);
    nextRangeStart = (nextRangeStart > rangeEnd ? rangeEnd : nextRangeStart);

    curRangeStart.getStr(curRangeStartStr);
    nextRangeStart.getStr(nextRangeStartStr);

    snprintf(buffer, bufSize, " [%s,%s): %s\n", curRangeStartStr.c_str(), nextRangeStartStr.c_str(), buckets[i]->toString().c_str());
    str.append(buffer);

    curRangeStart = nextRangeStart;
  }

  snprintf(buffer, bufSize, "Above bucket range: %s", buckets[bucketCount + 1]->toString().c_str());
  str.append(buffer);

  return true;
}

bool RankBucketAgg::print() {
  string str;

  if (!getStr(str)) {
    return false;
  }

  printf("buckets: %s\n", str.c_str());

  return true;
}

int RankBucketAgg::getBucketFromValue(Value& val) {
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
    return 0;
  }
}


bool RankBucketAgg::isSynthetic() {
  return false;
}

bool RankBucketAgg::getRealAggregates(vector<AggregateFunction*>& aggregates) {
  return true;
}

bool RankBucketAgg::fetchAggregates(Probe* probe) {
  return true;
}

