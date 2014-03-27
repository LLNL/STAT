#include <DysectAPI.h>
#include <libDysectAPI/src/expr-parser.tab.hh>
#include <libDysectAPI/src/lex.yy.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;
using namespace Stackwalker;

DysectAPI::DysectErrorCode ExprTree::evaluate(ConditionResult& conditionResult, Process::const_ptr process, THR_ID tid) {
  assert(!"Frontend cannot evaluate expression tree");
  return Error;
}


DysectAPI::DysectErrorCode TargetVar::getValue(ConditionResult& result, Value& c, Process::const_ptr process, THR_ID tid) {
  return Error;
}

DysectAPI::DysectErrorCode ExprNode::getGlobalVars(vector<GlobalState*>& globals) {
  return Error;
}
