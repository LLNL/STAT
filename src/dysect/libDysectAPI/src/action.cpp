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

#include <LibDysectAPI.h>

#include <DysectAPI/Err.h>
#include <DysectAPI/Action.h>
#include <DysectAPI/Aggregates/RankListAgg.h>
#include <DysectAPI/Aggregates/CmpAgg.h>
#include <DysectAPI/Aggregates/DescVar.h>


using namespace std;
using namespace DysectAPI;

//
// Act class statics
//

int DysectAPI::Act::aggregateIdCounter = 0;
//map<int, DysectAPI::Act*> DysectAPI::Act::aggregateMap;

string Act::str() {
  string returnString = "";

  switch(type) {
    case unknownAggType:
      returnString += "Unknown";
      break;
    case traceType:
      returnString += "Trace";
      break;
    case statType:
      returnString += "STAT";
      break;
    case detachAllType:
      returnString += "Detach All";
      break;
    case stackTraceType:
      returnString += "Stack Trace";
      break;
    case detachType:
      returnString += "Detach";
      break;
    case totalviewType:
      returnString += "TotalView";
      break;
    case depositCoreType:
      returnString += "Deposit Core";
      break;
    case loadLibraryType:
      returnString += "Load Library";
      break;
    case writeModuleVariableType:
      returnString += "Write Module Variable";
      break;
    case signalType:
      returnString += "Signal";
      break;
    case irpcType:
      returnString += "Irpc";
      break;
    case nullType:
      returnString += "No Op";
      break;
  }
  returnString += "(";
  returnString += stringRepr;
  returnString += ")";
  return returnString;
}

DysectAPI::Act* Act::loadLibrary(string library) {
  return new LoadLibrary(library);
}

DysectAPI::Act* Act::irpc(string functionName, string libraryPath, void *value, int size) {
  void *val;

  val = malloc(size);
  if (val == NULL) {
    DYSECTVERBOSE(true, "irpc failed to malloc %d bytes", size);
    return NULL;
  }
  memcpy(val, value, size);
  return new Irpc(functionName, libraryPath, val, size);
}

DysectAPI::Act* Act::writeModuleVariable(string libraryPath, string variableName, void *value, int size) {
  void *val;

  val = malloc(size);
  if (val == NULL) {
    DYSECTVERBOSE(true, "writeModuleVariable failed to malloc %d bytes", size);
    return NULL;
  }
  memcpy(val, value, size);
  return new WriteModuleVariable(libraryPath, variableName, val, size);
}

DysectAPI::Act* Act::signal(int sigNum) {
  return new Signal(sigNum);
}

DysectAPI::Act* Act::depositCore() {
  return new DepositCore();
}

DysectAPI::Act* Act::null() {
  return new Null();
}

DysectAPI::Act* Act::totalview() {
  return new Totalview();
}

DysectAPI::Act* Act::stat(AggScope scope, int traces, int frequency, bool threads) {
  return new Stat(scope, traces, frequency, threads);
}

DysectAPI::Act* Act::trace(string str) {
  return new Trace(str);
}

DysectAPI::Act* Act::stackTrace() {
  return new StackTrace();
}

DysectAPI::Act* Act::fullStackTrace() {
  return new FullStackTrace();
}

DysectAPI::Act* Act::startTrace(DataTrace* trace) {
  return new StartTrace(trace);
}

DysectAPI::Act* Act::stopTrace(DataTrace* trace) {
  return new StopTrace(trace);
}

DysectAPI::Act* Act::detachAll(AggScope scope) {
  return new DetachAll(scope);
}

DysectAPI::Act* Act::detach() {
  return new Detach();
}

DysectAPI::Act::Act() : category(unknownCategory),
                        type(unknownAggType) {

  id = aggregateIdCounter++;
  //aggregateMap.insert(pair<int, Act*>(id, this));
}

void Act::resetAggregateIdCounter() {
  aggregateIdCounter = 0;
}

LoadLibrary::LoadLibrary(string library) : library(library) {
  type = loadLibraryType;
}

bool LoadLibrary::prepare() {
  return true;
}

WriteModuleVariable::WriteModuleVariable(string libraryPath, string variableName, void *value, int size) : variableName(variableName), libraryPath(libraryPath), value(value), size(size) {
  type = writeModuleVariableType;
  stringRepr += libraryPath;
  stringRepr += ", ";
  stringRepr += variableName;
}

bool WriteModuleVariable::prepare() {
  return true;
}


Irpc::Irpc(string libraryPath, string functionName, void *value, int size) : functionName(functionName), libraryPath(libraryPath), value(value), size(size) {
  type = irpcType;
  stringRepr += libraryPath;
  stringRepr += ", ";
  stringRepr += functionName;
}

bool Irpc::prepare() {
  return true;
}

Signal::Signal(int sigNum) : sigNum(sigNum) {
  type = signalType;
}

bool Signal::prepare() {
  return true;
}

DepositCore::DepositCore() {
  type = depositCoreType;
}

bool DepositCore::prepare() {
  findAggregates();
  return true;
}

bool DepositCore::findAggregates() {
  AggregateFunction* aggFunc = 0;
  aggFunc = new RankListAgg(owner);
  aggregates.push_back(aggFunc);
  DYSECTVERBOSE(true, "Aggregate id: %d", aggFunc->getId());

  return true;
}


Null::Null() {
  type = nullType;
}

bool Null::prepare() {
  return true;
}


Totalview::Totalview() {
  type = totalviewType;
}

bool Totalview::prepare() {
  findAggregates();
  return true;
}

bool Totalview::findAggregates() {
  AggregateFunction* aggFunc = 0;
  aggFunc = new RankListAgg(owner);
  aggregates.push_back(aggFunc);
  DYSECTVERBOSE(true, "Aggregate id: %d", aggFunc->getId());

  return true;
}

Stat::Stat(AggScope scope, int traces, int frequency, bool threads) : traces(traces), frequency(frequency), threads(threads) {
  type = statType;
  lscope = scope;
  char buf[1024];
  snprintf(buf, 1024, "%d, %d,", traces, frequency);
  stringRepr += buf;
  if (!threads)
    stringRepr += " no";
  stringRepr += " threads";
}

bool Stat::prepare() {
  return true;
}

StackTrace::StackTrace() {
  type = stackTraceType;
  lscope = SatisfyingProcs;
}

bool StackTrace::prepare() {
  traces = new StackTraces();
  return true;
}

StartTrace::StartTrace(DataTrace* trace) : trace(trace) {

}

bool StartTrace::prepare() {
  return true;
}

std::map<MRN::Stream*, StopTrace*> StopTrace::waitingForResults;

StopTrace::StopTrace(DataTrace* trace) : trace(trace) {

}

bool StopTrace::prepare() {
  return true;
}

FullStackTrace::FullStackTrace() {
  type = fullStackTraceType;
  lscope = SatisfyingProcs;
}

bool FullStackTrace::prepare() {
  traces = new DataStackTrace();
  return true;
}

Trace::Trace(string str) : str(str) {
  type = traceType;
  lscope = SatisfyingProcs;
  stringRepr += str;
}


bool Trace::prepare() {

  DYSECTVERBOSE(true, "Preparing trace message: '%s'", str.c_str());

  findAggregates();

  return true;
}

bool Trace::findAggregates() {
  // Less expressive parser to find '@' <aggregate> ( "target var" ) expressions

  vector<pair<string, string> > foundAggregates;
  const char *cstr = str.c_str();
  int len = str.size();

  enum parser_state_t {
    text,
    aggName,
    dataExpr
  } parser_state = text;

  string aggNameStr = "";
  string dataExprStr = "";
  string nonAggStr = "";

  for(int i = 0; i < len; i++) {
    char c = cstr[i];

    if(c == '@') {
      if(parser_state != text) {
        return DYSECTWARN(false, "Trace string parser error: '@' denotes aggregate function");
      }

      strParts.push_back(pair<bool, string>(true, string(nonAggStr)));
      nonAggStr = "";

      aggNameStr = "";
      parser_state = aggName;
      continue;
    }

    if((parser_state == aggName) && (c == '(')) {
      if(aggNameStr.size() <= 0) {
        return DYSECTWARN(false, "Aggregate function name cannot be empty");
      }

      dataExprStr = "";
      parser_state = dataExpr;
      continue;

    } else if (parser_state == aggName) {
      aggNameStr += c;
    }

    if((parser_state == dataExpr) && (c == ')')) {

      if(aggNameStr.size() <= 0) {
        return DYSECTWARN(false, "Aggregate function name cannot be empty");
      }

      strParts.push_back(pair<bool, string>(false, ""));
      foundAggregates.push_back(pair<string, string>(string(aggNameStr), string(dataExprStr)));

      parser_state = text;
      continue;

    } else if(parser_state == dataExpr) {
      dataExprStr += c;
      continue;
    }

    if(parser_state == text) {
      nonAggStr += c;
    }
  }

  // Deal with left over
  if(nonAggStr.size() >= 1) {
    strParts.push_back(pair<bool, string>(true, string(nonAggStr)));
  }

  // Create aggregate function instances
  DYSECTVERBOSE(true, "Found aggregates: ");
  vector< pair<string, string> >::iterator aggIter = foundAggregates.begin();
  for(int i = 0; aggIter != foundAggregates.end(); aggIter++, i++) {
    string& curAggName = aggIter->first;
    string& curDataExpr = aggIter->second;

    DYSECTVERBOSE(true, "%d: %s(%s)", i, curAggName.c_str(), curDataExpr.c_str());

    int type;
    if(!Agg::aggregateIdFromName(curAggName, type)) {
      return DYSECTWARN(false, "Unknown aggregate function '%s'", curAggName.c_str());
    }

    AggregateFunction* aggFunc = 0;

    switch(type) {
      case minAgg:
        // %d is a placeholder since we don't know the type yet
        aggFunc = new Min("%d", curDataExpr.c_str());
      break;
      case maxAgg:
        aggFunc = new Max("%d", curDataExpr.c_str());
      break;
      case firstAgg:
        aggFunc = new First("%d", curDataExpr.c_str());
      break;
      case lastAgg:
        aggFunc = new Last("%d", curDataExpr.c_str());
      break;
      case funcLocAgg:
        aggFunc = new FuncLocation();
      break;
      case fileLocAgg:
        aggFunc = new FileLocation();
      break;
      case paramNamesAgg:
        aggFunc = new FuncParamNames();
      break;
      case tracesAgg:
        aggFunc = new StackTraces();
      break;
      case descAgg:
        aggFunc = new DescribeVariable(curDataExpr);
      break;
      case rankListAgg:
        aggFunc = new RankListAgg(owner);
      break;
      case timeListAgg:
        aggFunc = new TimeListAgg(owner);
      break;
      case bucketAgg:
        aggFunc = new BucketAgg(owner, curDataExpr.c_str());
      break;
      case rankBucketAgg:
        aggFunc = new RankBucketAgg(owner, curDataExpr.c_str());
      break;
      case dataTracesAgg:
        aggFunc = new DataStackTrace();
      break;
      default:
        DYSECTWARN(false, "Unsupported aggregate function '%s'", curAggName.c_str());
      break;
    }

    aggregates.push_back(aggFunc);
    DYSECTVERBOSE(true, "Aggregate id: %d", aggFunc->getId());
  }

  return true;
}


DetachAll::DetachAll(AggScope scope) {
  type = detachAllType;
  lscope = scope;
}

Detach::Detach() {
  type = detachType;
}

vector<Act*> DysectAPI::Acts(Act* act1, Act* act2, Act* act3, Act* act4, Act* act5, Act* act6, Act* act7, Act* act8) {
  vector<Act*> actions;

  if(act1)
    actions.push_back(act1);

  if(act2)
    actions.push_back(act2);

  if(act3)
    actions.push_back(act3);

  if(act4)
    actions.push_back(act4);

  if(act5)
    actions.push_back(act5);

  if(act6)
    actions.push_back(act6);

  if(act7)
    actions.push_back(act7);

  if(act8)
    actions.push_back(act8);


  return actions;
}
