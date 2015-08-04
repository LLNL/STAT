
#include <LibDysectAPI.h>

#include "BPatch.h"
#include "BPatch_function.h"
#include "BPatch_point.h"
#include "BPatch_binaryEdit.h"
#include "BPatch_function.h"
#include "BPatch_flowGraph.h"
#include "BPatch_statement.h"

#include "walker.h"

#include <DysectAPI/TraceAPIInstr.h>

using namespace std;

/************ Actions ************/
CollectValuesInstr::CollectValuesInstr(string variableName, int bufSize, bool allValues)
  : variableName(variableName), bufSize(bufSize), allValues(allValues) {
  collector = 0;
  variable = 0;
  buffer = 0;
  bufferIndex = 0;
}

bool CollectValuesInstr::prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function) {
  // Prepare the instrumented function call
  vector<BPatch_variableExpr*> variables;
  function->findVariable(variableName.c_str(), variables);

  if (variables.size() == 0) {
    cout << "Could not find '" << variableName << "' in function " << function->getName() << "!" << endl;
    return false;
  }

  // TODO: Multiple variables with the same name should all be instrumented
  cout << "Found variable '" << variableName << "'!" << endl;
  variable = variables[0];

  // Check the global variables have been allocated yet
  if (collector == 0) {
    buffer = target.addrHandle->malloc(bufSize);
    bufferIndex = target.addrHandle->malloc(sizeof(int));
    bufferSize = target.addrHandle->malloc(sizeof(int));      

    int initIndex = 0;
    bufferIndex->writeValue((void*)&initIndex, (int)sizeof(int));
    bufferSize->writeValue((void*)&bufSize, (int)sizeof(int));
    
    // Prepare the instrumentation snippet
    vector<BPatch_function*> collectValueFuncs;
    string collectorName = (allValues ? "collectAnyValue" : "collectValue");

    target.appImage->findFunction(collectorName.c_str(), collectValueFuncs);
  
    if (collectValueFuncs.size() != 1) {
      cout << "Could not find" << collectorName << "! " << collectValueFuncs.size() << endl;
      return false;
    } else {
      cout << "Found " << collectorName << "!" << endl;
    }

    collector = collectValueFuncs[0];
  }

  return true;
}

BPatch_snippet* CollectValuesInstr::getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint) {
  // Create instrumentation snippet
  vector<BPatch_snippet*> logArgs;
  logArgs.push_back(variable);
  logArgs.push_back(new BPatch_constExpr(buffer->getBaseAddr()));
  logArgs.push_back(new BPatch_constExpr(bufferIndex->getBaseAddr()));
  logArgs.push_back(new BPatch_constExpr(bufferSize->getBaseAddr()));

  return new BPatch_funcCallExpr(*collector, logArgs);
}

void CollectValuesInstr::finishAnalysis(struct instTarget& target) {
  int* localBuffer = new int[bufSize];
  int writtenBytes;

  bufferIndex->readValue(&writtenBytes, sizeof(int));
  if (writtenBytes == 0) {
    cout << "The variable " << variableName << " was never read!" << endl;
    return;
  }
    
  buffer->readValue(localBuffer, writtenBytes);
  cout << "The variable " << variableName << " took the values: " << endl << "    ";

  int writtenInts = writtenBytes / sizeof(int);
  for (int i = 0; i < writtenInts; i++) {
    cout << localBuffer[i] << " ";
  }

  cout << endl;
    
  delete[] localBuffer;
}

InvariantGeneratorInstr::InvariantGeneratorInstr(string variableName) : variableName(variableName) {
  collector = 0;
  variable = 0;
  orgVal = 0;
  modifications = 0;
}

bool InvariantGeneratorInstr::prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function) {
  // Prepare the instrumented function call
  vector<BPatch_variableExpr*> variables;
  function->findVariable(variableName.c_str(), variables);

  if (variables.size() == 0) {
    cout << "Could not find '" << variableName << "' in function " << function->getName() << "!" << endl;
    return false;
  }

  // TODO: Multiple variables with the same name should all be instrumented
  cout << "Found variable '" << variableName << "'!" << endl;
  variable = variables[0];

  // Check the global variables have been allocated yet
  if (collector == 0) {
    orgVal = target.addrHandle->malloc(*(variable->getType()));
    modifications = target.addrHandle->malloc(*(variable->getType()));
    initialized = target.addrHandle->malloc(1);      

    char initialVal = 0;
    initialized->writeValue(&initialVal, 1);
    
    // Prepare the instrumentation snippet
    vector<BPatch_function*> updateInvariantFuncs;
    target.appImage->findFunction("updateInvariant", updateInvariantFuncs);
  
    if (updateInvariantFuncs.size() != 1) {
      cout << "Could not find updateInvariant! " << updateInvariantFuncs.size() << endl;
      return false;
    } else {
      cout << "Found updateInvariant!" << endl;
    }

    collector = updateInvariantFuncs[0];
  }

  return true;
}
  
BPatch_snippet* InvariantGeneratorInstr::getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint) {
  // Create instrumentation snippet
  vector<BPatch_snippet*> logArgs;
  logArgs.push_back(variable);
  logArgs.push_back(new BPatch_constExpr(orgVal->getBaseAddr()));
  logArgs.push_back(new BPatch_constExpr(modifications->getBaseAddr()));
  logArgs.push_back(new BPatch_constExpr(initialized->getBaseAddr()));

  return new BPatch_funcCallExpr(*collector, logArgs);
}
  
void InvariantGeneratorInstr::finishAnalysis(struct instTarget& target) {
  int oval;
  int mods;
  char valid;

  initialized->readValue(&valid, sizeof(char));
  if (!valid) {
    cout << "The value " << variableName << " was never read." << endl;
    return;
  }
    
  orgVal->readValue(&oval, sizeof(int));
  modifications->readValue(&mods, sizeof(int));
    
  cout << "The original value of " << variableName << " was 0x" << hex << oval << endl;
  cout << "   The chages made are 0x" << mods << dec << endl;
}
  
ExtractFeaturesInstr::ExtractFeaturesInstr(string variableName) : variableName(variableName) {
  collector = 0;
  features = 0;
  min = 0;
  max = 0;
}

bool ExtractFeaturesInstr::prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function) {
  // Prepare the instrumented function call
  vector<BPatch_variableExpr*> variables;
  function->findVariable(variableName.c_str(), variables);

  if (variables.size() == 0) {
    cout << "Could not find '" << variableName << "' in function " << function->getName() << "!" << endl;
    return false;
  }

  // TODO: Multiple variables with the same name should all be instrumented
  cout << "Found variable '" << variableName << "'!" << endl;
  variable = variables[0];

  // Check the global variables have been allocated yet
  if (features == 0) {
    min = target.addrHandle->malloc(*(variable->getType()));
    max = target.addrHandle->malloc(*(variable->getType()));
    features = target.addrHandle->malloc(1);

    char initialVal = 0;
    features->writeValue(&initialVal, 1);
    
    // Prepare the instrumentation snippet
    vector<BPatch_function*> collectFeaturesFuncs;
    target.appImage->findFunction("collectFeatures", collectFeaturesFuncs);
  
    if (collectFeaturesFuncs.size() != 1) {
      cout << "Could not find collectFeatures! " << collectFeaturesFuncs.size() << endl;
      return false;
    } else {
      cout << "Found collectFeatures!" << endl;
    }

    collector = collectFeaturesFuncs[0];
  }

  return true;
}

BPatch_snippet* ExtractFeaturesInstr::getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint) {
  // Create instrumentation snippet
  vector<BPatch_snippet*> logArgs;
  logArgs.push_back(variable);
  logArgs.push_back(new BPatch_constExpr(min->getBaseAddr()));
  logArgs.push_back(new BPatch_constExpr(max->getBaseAddr()));
  logArgs.push_back(new BPatch_constExpr(features->getBaseAddr()));

  return new BPatch_funcCallExpr(*collector, logArgs);
}

void ExtractFeaturesInstr::finishAnalysis(struct instTarget& target) {
  char feat;
  int minVal, maxVal;

  features->readValue(&feat, 1);
  min->readValue(&minVal, sizeof(int));
  max->readValue(&maxVal, sizeof(int));

  if (feat & initialized) {
    if (feat & unchanged) { 
      cout << "The variable " << variableName << " was written once." << endl;	
    } else {
      cout << "The variable " << variableName << " was written multiple times." << endl;
    }
  } else {
    cout << "The variable " << variableName << " was never written." << endl;
    return;
  }

  if (feat & nonzero) {
    cout << variableName << " was never zero." << endl;
  } else {
    cout << variableName << " was zero." << endl;
  }

  if (feat & nonnegative) {
    cout << variableName << " was never negative." << endl;
  } else {
    cout << variableName << " was negative." << endl;
  }

  if (feat & increasing) {
    cout << variableName << " was monotonically increasing." << endl;
  } else {
    cout << variableName << " was not monotonically increasing." << endl;
  }

  if (feat & decreasing) {
    cout << variableName << " was monotonically decreasing." << endl;
  } else {
    cout << variableName << " was not monotonically decreasing." << endl;
  }

  cout << variableName << " was in [" << minVal << ":" << maxVal << "]" << endl;
}

PrintChangesInstr::PrintChangesInstr(string variableName) : variableName(variableName) {
  lastVal = 0;
}

void PrintChangesInstr::finishAnalysis(struct instTarget& target) {
  // Analysis results are already printed on the run
}
  
bool PrintChangesInstr::prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function) {
  // Prepare the instrumented function call
  vector<BPatch_variableExpr*> variables;
  function->findVariable(variableName.c_str(), variables);

  if (variables.size() == 0) {
    cout << "Could not find '" << variableName << "' in function " << function->getName() << "!" << endl;
    return false;
  }

  // TODO: Multiple variables with the same name should all be instrumented
  cout << "Found variable '" << variableName << "'!" << endl;
  variable = variables[0];

  // Check that room for aggregator has been allocated
  if (lastVal == 0) {
    lastVal = target.addrHandle->malloc(*(variable->getType()));

    // Prepare the instrumentation snippet
    vector<BPatch_function*> logAddrFunctions;
    target.appImage->findFunction("logValueAddr", logAddrFunctions);
  
    if (logAddrFunctions.size() != 1) {
      cout << "Could not find logValueAddr! " << logAddrFunctions.size() << endl;
      return false;
    } else {
      cout << "Found logValueAddr!" << endl;
    }

    logAddrFunc = logAddrFunctions[0];
  }

  return true;
}
  
BPatch_snippet* PrintChangesInstr::getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint) {
  // Create instrumentation snippet
  vector<BPatch_snippet*> logArgs;
  logArgs.push_back(new BPatch_constExpr(variableName.c_str()));
  logArgs.push_back(new BPatch_constExpr(lastVal->getBaseAddr()));
  logArgs.push_back(variable);
  logArgs.push_back(new BPatch_constExpr(instrumentationPoint->getAddress()));

  return new BPatch_funcCallExpr(*logAddrFunc, logArgs);
}

AnalysisInstr* AnalysisInstr::printChanges(string variable) {
  return new PrintChangesInstr(variable);
}

AnalysisInstr* AnalysisInstr::extractFeatures(string variable) {
  return new ExtractFeaturesInstr(variable);
}

AnalysisInstr* AnalysisInstr::generateInvariant(string variable) {
  return new InvariantGeneratorInstr(variable);
}

AnalysisInstr* AnalysisInstr:: collectValues(string variableName) {
  return new CollectValuesInstr(variableName);
}

/************ Scope ************/
FunctionScopeInstr::FunctionScopeInstr(int maxCallPath) : maxCallPath(maxCallPath) {
    
}
  
bool FunctionScopeInstr::shouldInstrument(vector<BPatch_function*>& instrumentedFunctions, BPatch_function* function) {
  return instrumentedFunctions.size() < maxCallPath;
}

CallPathScopeInstr::CallPathScopeInstr(vector<string> callPath) : callPath(callPath) {

}

bool CallPathScopeInstr::shouldInstrument(vector<BPatch_function*>& instrumentedFunctions, BPatch_function* function) {
  for (int i = 0; i < instrumentedFunctions.size(); i++) {
    if ((i == callPath.size()) ||
	(instrumentedFunctions[i]->getName().compare(callPath[i]))) {
      return false;
    }
  }

  return (callPath[instrumentedFunctions.size()].compare(function->getName()) == 0);
}

ScopeInstr* ScopeInstr::singleFunction() {
  return new FunctionScopeInstr(1);
}

ScopeInstr* ScopeInstr::reachableFunctions(int calls) {
  return new FunctionScopeInstr(calls);
}

ScopeInstr* ScopeInstr::callPath(string f1, string f2, string f3,
				 string f4, string f5, string f6,
				 string f7, string f8, string f9) {
  string functions [] = { f1, f2, f3, f4, f5, f6, f7, f8, f9 };
  vector<string> callPath;

  for (int i = 0; i < 9; i++) {
    if (functions[i].compare("") != 0) {
      callPath.push_back(functions[i]);
    } else {
      break;
    }
  }

  return ScopeInstr::callPath(callPath);
}

ScopeInstr* ScopeInstr::callPath(vector<string> callPath) {
  return new CallPathScopeInstr(callPath);
}

/************ Sampling points ************/
MultipleSamplingPointsInstr::MultipleSamplingPointsInstr(vector<SamplingPointsInstr*> pointGenerators) : pointGenerators(pointGenerators) {
  
}

vector<BPatch_point*> MultipleSamplingPointsInstr::getInstrumentationPoints(struct instTarget& target, BPatch_function* function) {
  vector<BPatch_point*> points;

  for (vector<SamplingPointsInstr*>::iterator it = pointGenerators.begin(); it != pointGenerators.end(); ++it) {
    vector<BPatch_point*> p = (*it)->getInstrumentationPoints(target, function);
      
    points.insert(points.end(), p.begin(), p.end());
  }
    
  return points;
}

StoreSamplingPointsInstr::StoreSamplingPointsInstr(SamplingTime time) : time(time) {

}

vector<BPatch_point*> StoreSamplingPointsInstr::getInstrumentationPoints(struct instTarget& target, BPatch_function* function) {
  // Not yet implemented
  throw exception();
}

LoopSamplingPointsInstr::LoopSamplingPointsInstr(SamplingTime time) : time(time) {
  
}

vector<BPatch_point*> LoopSamplingPointsInstr::getInstrumentationPoints(struct instTarget& target, BPatch_function* function) {
  // Not yet implemented
  throw exception();
}

FunctionSamplingPointsInstr::FunctionSamplingPointsInstr(SamplingTime time) : time(time) {

}

vector<BPatch_point*> FunctionSamplingPointsInstr::getInstrumentationPoints(struct instTarget& target, BPatch_function* function) {
  vector<BPatch_point*> points;
  vector<BPatch_point*>* functionPoints = new vector<BPatch_point*>();

  switch(time) {
  case beginning:
    functionPoints = function->findPoint(BPatch_entry);
    break;
  case end:
    functionPoints = function->findPoint(BPatch_exit);
    break;
  default:
    // Error: Unsupported time
    return points;
  }

  // Copy the points to our vector
  for (vector<BPatch_point*>::iterator fp = functionPoints->begin(); fp != functionPoints->end(); ++fp) {
    points.push_back(*fp);
  }

  return points;
}

FunctionCallSamplingPointsInstr::FunctionCallSamplingPointsInstr(SamplingTime time) : time(time) {

}

vector<BPatch_point*> FunctionCallSamplingPointsInstr::getInstrumentationPoints(struct instTarget& target, BPatch_function* function) {
  vector<BPatch_point*> points;
  vector<BPatch_point*>* subCalls = function->findPoint(BPatch_subroutine);
    
  if (subCalls == 0) {
    // Error: functions not found?
    return points;
  }

  if (time != before) {
    // After not yet implemented, others invalid
    return points;
  }

  // Copy the points to our vector
  for (vector<BPatch_point*>::iterator call = subCalls->begin(); call != subCalls->end(); ++call) {
    points.push_back(*call);
  }
    
  return points;
}

BasicBlockSamplingPointsInstr::BasicBlockSamplingPointsInstr(SamplingTime time) : time(time) {
  
}

vector<BPatch_point*> BasicBlockSamplingPointsInstr::getInstrumentationPoints(struct instTarget& target, BPatch_function* function) {
  vector<BPatch_point*> points;
    
  BPatch_flowGraph* cfg = function->getCFG();
  set<BPatch_basicBlock*> basicBlocks;

  cfg->getAllBasicBlocks(basicBlocks);
  if (basicBlocks.size() == 0) {
    // Error: 
    cout << "Found no basic blocks!" << endl;
    return points;
  } else {
    cout << "Found " << basicBlocks.size() << " basic blocks in "
	 << function->getName() << "!" << endl;
  }

  for (set<BPatch_basicBlock*>::iterator it = basicBlocks.begin(); it != basicBlocks.end(); ++it) {
    if (!(*it)->isEntryBlock() &&
	!(*it)->isExitBlock()) {
      // Instrument the point
      switch(time) {
      case beginning:
	points.push_back((*it)->findEntryPoint());
	break;
      case end:
	points.push_back((*it)->findExitPoint());
	break;
      default:
	// Error: unsupported time
	return points;
      }
    }
  }

  return points;
}

SamplingPointsInstr* SamplingPointsInstr::stores(SamplingTime time) {
  return new StoreSamplingPointsInstr(time);
}

SamplingPointsInstr* SamplingPointsInstr::loop(SamplingTime time) {
  return new LoopSamplingPointsInstr(time);
}

SamplingPointsInstr* SamplingPointsInstr::function(SamplingTime time) {
  return new FunctionSamplingPointsInstr(time);
}

SamplingPointsInstr* SamplingPointsInstr::functionCall(SamplingTime time) {
  return new FunctionCallSamplingPointsInstr(time);
}

SamplingPointsInstr* SamplingPointsInstr::basicBlocks(SamplingTime time) {
  return new BasicBlockSamplingPointsInstr(time);
}

SamplingPointsInstr* SamplingPointsInstr::multiple(
		         SamplingPointsInstr* sp1, SamplingPointsInstr* sp2, SamplingPointsInstr* sp3,
		         SamplingPointsInstr* sp4, SamplingPointsInstr* sp5, SamplingPointsInstr* sp6,
		         SamplingPointsInstr* sp7, SamplingPointsInstr* sp8, SamplingPointsInstr* sp9) {
  SamplingPointsInstr* generators [] = { sp1, sp2, sp3, sp4, sp5, sp6, sp7, sp8, sp9 };
  vector<SamplingPointsInstr*> pointGenerators;

  for (int i = 0; i < 9; i++) {
    if (generators[i] != 0) {
      pointGenerators.push_back(generators[i]);
    } else {
      break;
    }
  }

  return new MultipleSamplingPointsInstr(pointGenerators);
}

/************ DataTrace ************/
void DataTraceInstr::install_recursive(struct instTarget& target, vector<BPatch_function*>& instrumentedFuncStack,
		                       BPatch_function* currentFunction) {
  if (instrumentedFunctions.count(currentFunction->getName()) != 0) {
    return;
  }
    
  if (scope->shouldInstrument(instrumentedFuncStack, currentFunction)) {
    cout << "[" << instrumentedFuncStack.size() << "] Instrumenting " << currentFunction->getName() << endl;
	
    // Instrument the current function
    if (analysis->prepareInstrumentedFunction(target, currentFunction)) {
      vector<BPatch_point*> analysisPoints = points->getInstrumentationPoints(target, currentFunction);

      for (vector<BPatch_point*>::iterator it = analysisPoints.begin(); it != analysisPoints.end(); ++it) {
	BPatch_snippet* analysisSnippet = analysis->getInstrumentationSnippet(target, *it);

	vector<BPatch_point*> analysisPoint;
	analysisPoint.push_back(*it);

	target.addrHandle->insertSnippet(*analysisSnippet, analysisPoint);
      }
    }

    instrumentedFunctions.insert(currentFunction->getName());
    instrumentedFuncStack.push_back(currentFunction);
      
    // Instrument subroutines
    vector<BPatch_point*>* subCalls = currentFunction->findPoint(BPatch_subroutine);

    if (subCalls != 0) {
      for (vector<BPatch_point*>::iterator call = subCalls->begin(); call != subCalls->end(); ++call) {
	BPatch_function* subRoutine = (*call)->getCalledFunction();

	if (subRoutine != 0) {
	  install_recursive(target, instrumentedFuncStack, subRoutine);
	}
      }
    }
      
    instrumentedFuncStack.pop_back();
  }
}
  
DataTraceInstr::DataTraceInstr(AnalysisInstr* analysis, ScopeInstr* scope, SamplingPointsInstr* points)
  : analysis(analysis), scope(scope), points(points) {
  
}

void DataTraceInstr::install(struct instTarget& target, BPatch_function* root_function) {
  vector<BPatch_function*> instrumentedFunctions;

  install_recursive(target, instrumentedFunctions, root_function);
}

void DataTraceInstr::finishAnalysis(struct instTarget& target) {
  analysis->finishAnalysis(target);
}

#ifdef PORT_LATER
BPatch TraceAPI::bpatch;
map<Dyninst::ProcControlAPI::Process::const_ptr, instTarget*> TraceAPI::procTable;

instTarget* TraceAPI::findProcess(Dyninst::ProcControlAPI::Process::const_ptr proc) {
  map<Dyninst::ProcControlAPI::Process::const_ptr, instTarget*>::iterator it;

  it = procTable.find(proc);

  if (it == procTable.end()) {
    return 0;
  } else {
    return it->second;
  }
}

bool TraceAPI::addProcess(Dyninst::ProcControlAPI::Process::const_ptr proc, int pid, string executable) {
  BPatch_process* procHandle = bpatch.processAttach(executable.c_str(), pid);

  if (procHandle == 0) {
    return false;
  }

  instTarget* target = new instTarget();
  target->addrHandle = dynamic_cast<BPatch_addressSpace*>(procHandle);
  target->appImage = target->addrHandle->getImage();

  procTable[proc] = target;

  return true;
}

bool TraceAPI::instrumentProcess(Dyninst::ProcControlAPI::Process::const_ptr proc, DataTrace* trace) {
  instTarget* target = TraceAPI::findProcess(proc);
  
  if (target == 0) {
    return false;
  }
  
  Stackwalker::Walker* walker = (Stackwalker::Walker*)(proc->getData());
  vector<Stackwalker::Frame> stackWalk;

  if (!walker->walkStack(stackWalk)) {
    return false;
  }

  string curFuncName;
  stackWalk[stackWalk.size() - 1].getName(curFuncName);

  vector<BPatch_function*> instrFunction;
  target->appImage->findFunction(curFuncName.c_str(), instrFunction);

  if (instrFunction.size() != 1) {
    return false;
  }

  trace->install(*target, instrFunction[0]);

  return true;
}

bool TraceAPI::finishAnalysis(Dyninst::ProcControlAPI::Process::const_ptr proc, DataTrace* trace) {
  instTarget* target = TraceAPI::findProcess(proc);
  
  if (target == 0) {
    return false;
  }
  
  trace->finishAnalysis(*target);

  return true;
}
#endif // PORT_LATER

