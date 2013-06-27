#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;

//
// Act class statics
//

int DysectAPI::Act::aggregateIdCounter = 0;
//map<int, DysectAPI::Act*> DysectAPI::Act::aggregateMap;


DysectAPI::Act* Act::stat(AggScope scope, int traces, int frequency, bool threads) {
  return new Stat(scope, traces, frequency, threads);
}

DysectAPI::Act* Act::trace(string str) {
  return new Trace(str);
}

DysectAPI::Act* Act::stackTrace() {
  return new StackTrace();
}

DysectAPI::Act* Act::detach(AggScope scope) {
  return new Detach(scope);
}

DysectAPI::Act::Act() : category(unknownCategory),
                        type(unknownAggType) {

  id = aggregateIdCounter++;
  //aggregateMap.insert(pair<int, Act*>(id, this));
}

Stat::Stat(AggScope scope, int traces, int frequency, bool threads) : traces(traces), frequency(frequency), threads(threads) {
  type = statType;
  lscope = scope;
}


StackTrace::StackTrace() {
  type = stackTraceType;
  lscope = SatisfyingProcs;
}

bool StackTrace::prepare() {
  traces = new StackTraces();
  return true;
}

Trace::Trace(string str) : str(str) {
  type = traceType;
  lscope = SatisfyingProcs;
}


bool Trace::prepare() {
  
  Err::verbose(true, "Preparing trace message: '%s'", str.c_str());

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
        return Err::warn(false, "Trace string parser error: '@' denotes aggregate function");
      }
      
      strParts.push_back(pair<bool, string>(true, string(nonAggStr)));
      nonAggStr = "";

      aggNameStr = "";
      parser_state = aggName;
      continue;
    }

    if((parser_state == aggName) && (c == '(')) {
      if(aggNameStr.size() <= 0) {
        return Err::warn(false, "Aggregate function name cannot be empty");
      }
      
      dataExprStr = "";
      parser_state = dataExpr;
      continue;

    } else if (parser_state == aggName) {
      aggNameStr += c;
    }

    if((parser_state == dataExpr) && (c == ')')) {
      
      if(aggNameStr.size() <= 0) {
        return Err::warn(false, "Aggregate function name cannot be empty");
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
  Err::verbose(true, "Found aggregates: ");
  vector< pair<string, string> >::iterator aggIter = foundAggregates.begin();
  for(int i = 0; aggIter != foundAggregates.end(); aggIter++, i++) {
    string& curAggName = aggIter->first;
    string& curDataExpr = aggIter->second;

    Err::verbose(true, "%d: %s(%s)", i, curAggName.c_str(), curDataExpr.c_str());

    int type;
    if(!Agg::aggregateIdFromName(curAggName, type)) {
      return Err::warn(false, "Unknown aggregate function '%s'", curAggName.c_str());
    }

    AggregateFunction* aggFunc = 0;

    switch(type) {
      case minAgg:
        // XXX: %d should be replaced with type specialization
        aggFunc = new Min("%d", curDataExpr.c_str());
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
      default:
        Err::warn(false, "Unsupported aggregate function '%s'", curAggName.c_str());
      break;
    }

    aggregates.push_back(aggFunc);
    Err::verbose(true, "Aggregate id: %d", aggFunc->getId());
  }

  return true;
}


Detach::Detach(AggScope scope) {
  type = detachType;
  lscope = scope;
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
