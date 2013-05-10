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


