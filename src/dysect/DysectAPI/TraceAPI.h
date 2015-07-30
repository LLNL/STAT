
#ifndef __TRACEAPI_H
#define __TRACEAPI_H

#include "BPatch.h"
#include "BPatch_function.h"
#include "BPatch_point.h"

#include "ProcessSet.h"

#include <set>

struct instTarget {
  BPatch_addressSpace* addrHandle;
  BPatch_image* appImage;
};

class Analysis {
public:
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function) = 0;
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint) = 0;
  virtual void finishAnalysis(struct instTarget& target) = 0;
  
  static Analysis* printChanges(string variableName);
  static Analysis* extractFeatures(string variableName);
  static Analysis* generateInvariant(string variableName);
  static Analysis* collectValues(string variableName);
};

class CollectValues : public Analysis {
  string variableName;
  const int bufSize;
  const bool allValues;
  
  BPatch_function* collector;
  BPatch_variableExpr* variable;
  BPatch_variableExpr* buffer;
  BPatch_variableExpr* bufferIndex;
  BPatch_variableExpr* bufferSize;
  
public:
  CollectValues(string variableName, int bufSize = 2048, bool allValues = false);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual void finishAnalysis(struct instTarget& target);
};

class InvariantGenerator : public Analysis {
  string variableName;
  BPatch_function* collector;
  BPatch_variableExpr* variable;
  BPatch_variableExpr* orgVal;
  BPatch_variableExpr* modifications;
  BPatch_variableExpr* initialized;
  
public:
  InvariantGenerator(string variableName);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual void finishAnalysis(struct instTarget& target);
};

class ExtractFeatures : public Analysis {
  string variableName;
  BPatch_function* collector;
  BPatch_variableExpr* variable;
  BPatch_variableExpr* min;
  BPatch_variableExpr* max;
  BPatch_variableExpr* features;

  enum bitmap {
    initialized = 0x1,
    nonzero     = 0x2,
    nonnegative = 0x4,
    increasing  = 0x8,
    decreasing  = 0x10,
    unchanged   = 0x20
  };  

public:
  ExtractFeatures(string variableName);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual void finishAnalysis(struct instTarget& target);
};

class PrintChanges : public Analysis {
  string variableName;
  BPatch_variableExpr* variable;
  BPatch_function* logAddrFunc;
  BPatch_variableExpr* lastVal;
  
public:
  PrintChanges(string variableName);
  virtual void finishAnalysis(struct instTarget& target);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
};

class Scope {
public:
  virtual bool shouldInstrument(vector<BPatch_function*>& instrumentedFunctions,
				BPatch_function* function) = 0;

  static Scope* singleFunction();
  static Scope* reachableFunctions(int calls = numeric_limits<int>::max());
  static Scope* callPath(string f1 = "", string f2 = "", string f3 = "",
			 string f4 = "", string f5 = "", string f6 = "",
			 string f7 = "", string f8 = "", string f9 = "");
};

class FunctionScope : public Scope {
  int maxCallPath;
  
public:
  FunctionScope(int maxCallPath);
  bool shouldInstrument(vector<BPatch_function*>& instrumentedFunctions, BPatch_function* function);
};

class CallPathScope : public Scope {
  vector<string> callPath;
  
public:
  CallPathScope(vector<string> callPath);
  virtual bool shouldInstrument(vector<BPatch_function*>& instrumentedFunctions, BPatch_function* function);
};

class SamplingPoints {
public:
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function) = 0;

  enum SamplingTime {
    beginning, end, before, after
  };
  
  static SamplingPoints* stores(SamplingTime time = before);
  static SamplingPoints* loop(SamplingTime time = end);
  static SamplingPoints* function(SamplingTime time = after);
  static SamplingPoints* functionCall(SamplingTime time = before);
  static SamplingPoints* basicBlocks(SamplingTime time = end);
  static SamplingPoints* multiple(SamplingPoints* sp1, SamplingPoints* sp2 = 0, SamplingPoints* sp3 = 0,
				  SamplingPoints* sp4 = 0, SamplingPoints* sp5 = 0, SamplingPoints* sp6 = 0,
				  SamplingPoints* sp7 = 0, SamplingPoints* sp8 = 0, SamplingPoints* sp9 = 0);
};

class MultipleSamplingPoints : public SamplingPoints {
  vector<SamplingPoints*> pointGenerators;

public:
  MultipleSamplingPoints(vector<SamplingPoints*> pointGenerators);
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class StoreSamplingPoints : public SamplingPoints {
  SamplingTime time;

public:
  StoreSamplingPoints(SamplingTime time);
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class LoopSamplingPoints : public SamplingPoints {
  SamplingTime time;

public:
  LoopSamplingPoints(SamplingTime time);
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class FunctionSamplingPoints : public SamplingPoints {
  SamplingTime time;

public:
  FunctionSamplingPoints(SamplingTime time);
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class FunctionCallSamplingPoints : public SamplingPoints {
  SamplingTime time;

public:
  FunctionCallSamplingPoints(SamplingTime time);
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class BasicBlockSamplingPoints : public SamplingPoints {
  SamplingTime time;

public:
  BasicBlockSamplingPoints(SamplingTime time);
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class DataTrace {
  Analysis* analysis;
  Scope* scope;
  SamplingPoints* points;
  
  set<string> instrumentedFunctions;

  void install_recursive(struct instTarget& target, vector<BPatch_function*>& instrumentedFuncStack,
			 BPatch_function* currentFunction);
  
public:
  DataTrace(Analysis* analysis, Scope* scope, SamplingPoints* points);
  void install(struct instTarget& target, BPatch_function* root_function);
  void finishAnalysis(struct instTarget& target);
};

class TraceAPI {
  static BPatch bpatch;
  static map<Dyninst::ProcControlAPI::Process::const_ptr, instTarget*> procTable;

 public:
  static bool addProcess(Dyninst::ProcControlAPI::Process::const_ptr proc, int pid, string executable);
  static instTarget* findProcess(Dyninst::ProcControlAPI::Process::const_ptr proc);
  static bool instrumentProcess(Dyninst::ProcControlAPI::Process::const_ptr proc, DataTrace* trace);
  static bool finishAnalysis(Dyninst::ProcControlAPI::Process::const_ptr proc, DataTrace* trace);
};


#endif
