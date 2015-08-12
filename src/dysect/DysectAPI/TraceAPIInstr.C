
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
#include <DysectAPI/Err.h>

#include <DysectAPI/Aggregates/Aggregate.h>
#include <DysectAPI/Aggregates/AggregateFunction.h>

using namespace std;
using namespace DysectAPI;

/************ Actions ************/
CollectValuesInstr::CollectValuesInstr(CollectValues* original, string variableName, int bufSize, bool allValues)
  : original(original), variableName(variableName), bufSize(bufSize), allValues(allValues) {
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
    ///OO cout << "Could not find '" << variableName << "' in function " << function->getName() << "!" << endl;
    return DYSECTWARN(false, "Could not find '%s' in function %s!", variableName.c_str(), function->getName().c_str());
  }

  // TODO: Multiple variables with the same name should all be instrumented
  ///OO cout << "Found variable '" << variableName << "'!" << endl;
  DYSECTVERBOSE(true, "Found variable '%s'!", variableName.c_str());
		
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
      ///OO cout << "Could not find" << collectorName << "! " << collectValueFuncs.size() << endl;
      return DYSECTWARN(false, "Could not find %s! %d", collectorName.c_str(), collectValueFuncs.size());
    } else {
      ///OO cout << "Found " << collectorName << "!" << endl;
      DYSECTVERBOSE(true, "Found %s!", collectorName.c_str());
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
  CollectValuesAgg* aggregator = original->getAggregator();
  int* localBuffer = new int[bufSize];
  int writtenBytes;

  bufferIndex->readValue(&writtenBytes, sizeof(int));
  if (writtenBytes == 0) {
    ///OO cout << "The variable " << variableName << " was never read!" << endl;
    return;
  }
    
  buffer->readValue(localBuffer, writtenBytes);
  ///OO cout << "The variable " << variableName << " took the values: " << endl << "    ";

  int writtenInts = writtenBytes / sizeof(int);
  for (int i = 0; i < writtenInts; i++) {
    ///OO cout << localBuffer[i] << " ";

    aggregator->addValue(localBuffer[i]);
  }

  ///OO cout << endl;
    
  delete[] localBuffer;
}

InvariantGeneratorInstr::InvariantGeneratorInstr(InvariantGenerator* original, string variableName)
  : original(original), variableName(variableName) {
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
  
ExtractFeaturesInstr::ExtractFeaturesInstr(ExtractFeatures* original, string variableName)
  : original(original), variableName(variableName) {
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

PrintChangesInstr::PrintChangesInstr(PrintChanges* original, string variableName)
  : original(original), variableName(variableName) {
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
    ///OO cout << "Found no basic blocks!" << endl;
    DYSECTWARN(false, "Found no basic blocks!");
    return points;
  } else {
    ///OO cout << "Found " << basicBlocks.size() << " basic blocks in "
    ///OO      << function->getName() << "!" << endl;
    DYSECTVERBOSE("Found %d basic blocks in %s!", basicBlocks.size(), function->getName().c_str());
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


/************ DataTrace ************/
void DataTraceInstr::install_recursive(struct instTarget& target, vector<BPatch_function*>& instrumentedFuncStack,
		                       BPatch_function* currentFunction) {
  if (instrumentedFunctions.count(currentFunction->getName()) != 0) {
    return;
  }
    
  if (scope->shouldInstrument(instrumentedFuncStack, currentFunction)) {
    ///OO cout << "[" << instrumentedFuncStack.size() << "] Instrumenting " << currentFunction->getName() << endl;
    DYSECTVERBOSE(true, "[%d] Instrumenting %s", instrumentedFuncStack.size(), currentFunction->getName().c_str());
	
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

bool DataTraceInstr::install(struct instTarget& target, std::string rootFuncName) {
  vector<BPatch_function*> instrumentedFunctions;
  
  vector<BPatch_function*> rootFunctions;
  target.appImage->findFunction(rootFuncName.c_str(), rootFunctions);

  if (rootFunctions.size() != 1) {
    return false;
  }

  install_recursive(target, instrumentedFunctions, rootFunctions[0]);

  return true;
}

void DataTraceInstr::finishAnalysis(struct instTarget& target) {
  analysis->finishAnalysis(target);
}



