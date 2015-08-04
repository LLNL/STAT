
#include <LibDysectAPI.h>

#include <DysectAPI/TraceAPI.h>

using namespace std;

/************ Analysis ************/
CollectValues::CollectValues(string variableName, int bufSize, bool allValues)
  : variableName(variableName), bufSize(bufSize), allValues(allValues) {

}

InvariantGenerator::InvariantGenerator(string variableName) : variableName(variableName) {

}

ExtractFeatures::ExtractFeatures(string variableName) : variableName(variableName) {

}

PrintChanges::PrintChanges(string variableName) : variableName(variableName) {

}

Analysis* Analysis::printChanges(string variable) {
  return new PrintChanges(variable);
}

Analysis* Analysis::extractFeatures(string variable) {
  return new ExtractFeatures(variable);
}

Analysis* Analysis::generateInvariant(string variable) {
  return new InvariantGenerator(variable);
}

Analysis* Analysis:: collectValues(string variableName) {
  return new CollectValues(variableName);
}

/************ Scope ************/
FunctionScope::FunctionScope(int maxCallPath) : maxCallPath(maxCallPath) {
    
}
  
CallPathScope::CallPathScope(vector<string> callPath) : callPath(callPath) {

}

Scope* Scope::singleFunction() {
  return new FunctionScope(1);
}

Scope* Scope::reachableFunctions(int calls) {
  return new FunctionScope(calls);
}

Scope* Scope::callPath(string f1, string f2, string f3,
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

  return new CallPathScope(callPath);
}

/************ SamplingPoint ************/
MultipleSamplingPoints::MultipleSamplingPoints(vector<SamplingPoints*> pointGenerators) : pointGenerators(pointGenerators) {
  
}

StoreSamplingPoints::StoreSamplingPoints(SamplingTime time) : time(time) {

}

LoopSamplingPoints::LoopSamplingPoints(SamplingTime time) : time(time) {
  
}

FunctionSamplingPoints::FunctionSamplingPoints(SamplingTime time) : time(time) {

}

FunctionCallSamplingPoints::FunctionCallSamplingPoints(SamplingTime time) : time(time) {

}

BasicBlockSamplingPoints::BasicBlockSamplingPoints(SamplingTime time) : time(time) {
  
}

SamplingPoints* SamplingPoints::stores(SamplingTime time) {
  return new StoreSamplingPoints(time);
}

SamplingPoints* SamplingPoints::loop(SamplingTime time) {
  return new LoopSamplingPoints(time);
}

SamplingPoints* SamplingPoints::function(SamplingTime time) {
  return new FunctionSamplingPoints(time);
}

SamplingPoints* SamplingPoints::functionCall(SamplingTime time) {
  return new FunctionCallSamplingPoints(time);
}

SamplingPoints* SamplingPoints::basicBlocks(SamplingTime time) {
  return new BasicBlockSamplingPoints(time);
}

SamplingPoints* SamplingPoints::multiple(SamplingPoints* sp1, SamplingPoints* sp2, SamplingPoints* sp3,
					 SamplingPoints* sp4, SamplingPoints* sp5, SamplingPoints* sp6,
					 SamplingPoints* sp7, SamplingPoints* sp8, SamplingPoints* sp9) {
  SamplingPoints* generators [] = { sp1, sp2, sp3, sp4, sp5, sp6, sp7, sp8, sp9 };
  vector<SamplingPoints*> pointGenerators;

  for (int i = 0; i < 9; i++) {
    if (generators[i] != 0) {
      pointGenerators.push_back(generators[i]);
    } else {
      break;
    }
  }

  return new MultipleSamplingPoints(pointGenerators);
}

/************ DataTrace ************/
DataTrace::DataTrace(Analysis* analysis, Scope* scope, SamplingPoints* points)
  : analysis(analysis), scope(scope), points(points) {

  createInstrumentor();
}

bool TraceAPI::instrumentProcess(Dyninst::ProcControlAPI::Process::const_ptr proc, DataTrace* trace) {
  // TODO: Instrument the function from here

  return true;
}

bool TraceAPI::finishAnalysis(Dyninst::ProcControlAPI::Process::const_ptr proc, DataTrace* trace) {
  // TODO: Finish analysis from here

  return true;
}
