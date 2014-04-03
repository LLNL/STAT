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

#include "DysectAPI/Aggregates/Aggregate.h"

using namespace std;
using namespace DysectAPI;

DescribeVariable::DescribeVariable(string name) : AggregateFunction(descAgg), varName(name) {
  strAgg = new StaticStr();
}

bool DescribeVariable::isSynthetic() {
  return true;
}

int DescribeVariable::getSize() {
  return 0;
}

int DescribeVariable::writeSubpacket(char *p) {
  return 0;
}

bool DescribeVariable::clear() {
  if(strAgg)
    strAgg->clear();

  outStr = "";
  varSpecs.clear();
  aggregates.clear();

  return true;
}

bool DescribeVariable::getStr(string& str) {
  str.append(outStr);

  return true;
}

bool DescribeVariable::print() {
  return true;
}

bool DescribeVariable::aggregate(AggregateFunction* agg) {
  return true;
}

bool DescribeVariable::getRealAggregates(vector<AggregateFunction*>& laggregates) {
  
  map<int, AggregateFunction*>::iterator aggIter = aggregates.begin();
  for(; aggIter != aggregates.end(); aggIter++) {
    AggregateFunction* aggFunc = aggIter->second;
    laggregates.push_back(aggFunc);
  }

  vector<string>::iterator strIter = varSpecs.begin();

  for(; strIter != varSpecs.end(); strIter++) {
    string varSpec = *strIter;
    strAgg->addStr(varSpec);
  }

  laggregates.push_back(strAgg);
  return true;
}


