
#ifndef __TRACEAPIINSTR_H
#define __TRACEAPIINSTR_H

#include "BPatch.h"
#include "BPatch_function.h"
#include "BPatch_point.h"

#include "ProcessSet.h"

#include <set>

struct instTarget {
  BPatch_addressSpace* addrHandle;
  BPatch_image* appImage;
};

class AnalysisInstr {
public:
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function) = 0;
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint) = 0;
  virtual void finishAnalysis(struct instTarget& target) = 0;
  
  static AnalysisInstr* printChanges(string variableName);
  static AnalysisInstr* extractFeatures(string variableName);
  static AnalysisInstr* generateInvariant(string variableName);
  static AnalysisInstr* collectValues(string variableName);
};

class CollectValuesInstr : public AnalysisInstr {
  string variableName;
  const int bufSize;
  const bool allValues;
  
  BPatch_function* collector;
  BPatch_variableExpr* variable;
  BPatch_variableExpr* buffer;
  BPatch_variableExpr* bufferIndex;
  BPatch_variableExpr* bufferSize;
  
public:
  CollectValuesInstr(string variableName, int bufSize = 2048, bool allValues = false);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual void finishAnalysis(struct instTarget& target);
};

class InvariantGeneratorInstr : public AnalysisInstr {
  string variableName;
  BPatch_function* collector;
  BPatch_variableExpr* variable;
  BPatch_variableExpr* orgVal;
  BPatch_variableExpr* modifications;
  BPatch_variableExpr* initialized;
  
public:
  InvariantGeneratorInstr(string variableName);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual void finishAnalysis(struct instTarget& target);
};

class ExtractFeaturesInstr : public AnalysisInstr {
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
  ExtractFeaturesInstr(string variableName);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual void finishAnalysis(struct instTarget& target);
};

class PrintChangesInstr : public AnalysisInstr {
  string variableName;
  BPatch_variableExpr* variable;
  BPatch_function* logAddrFunc;
  BPatch_variableExpr* lastVal;
  
public:
  PrintChangesInstr(string variableName);
  virtual void finishAnalysis(struct instTarget& target);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
};

class ScopeInstr {
public:
  virtual bool shouldInstrument(vector<BPatch_function*>& instrumentedFunctions,
				BPatch_function* function) = 0;

  static ScopeInstr* singleFunction();
  static ScopeInstr* reachableFunctions(int calls = numeric_limits<int>::max());
  static ScopeInstr* callPath(string f1 = "", string f2 = "", string f3 = "",
			      string f4 = "", string f5 = "", string f6 = "",
			      string f7 = "", string f8 = "", string f9 = "");
  static ScopeInstr* callPath(vector<string> callPath);
};

class FunctionScopeInstr : public ScopeInstr {
  int maxCallPath;
  
public:
  FunctionScopeInstr(int maxCallPath);
  bool shouldInstrument(vector<BPatch_function*>& instrumentedFunctions, BPatch_function* function);
};

class CallPathScopeInstr : public ScopeInstr {
  vector<string> callPath;
  
public:
  CallPathScopeInstr(vector<string> callPath);
  virtual bool shouldInstrument(vector<BPatch_function*>& instrumentedFunctions, BPatch_function* function);
};

class SamplingPointsInstr {
public:
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function) = 0;

  enum SamplingTime {
    beginning, end, before, after
  };
  
  static SamplingPointsInstr* stores(SamplingTime time = before);
  static SamplingPointsInstr* loop(SamplingTime time = end);
  static SamplingPointsInstr* function(SamplingTime time = after);
  static SamplingPointsInstr* functionCall(SamplingTime time = before);
  static SamplingPointsInstr* basicBlocks(SamplingTime time = end);
  static SamplingPointsInstr* multiple(
			           SamplingPointsInstr* sp1,     SamplingPointsInstr* sp2 = 0, SamplingPointsInstr* sp3 = 0,
			           SamplingPointsInstr* sp4 = 0, SamplingPointsInstr* sp5 = 0, SamplingPointsInstr* sp6 = 0,
			           SamplingPointsInstr* sp7 = 0, SamplingPointsInstr* sp8 = 0, SamplingPointsInstr* sp9 = 0);
};

class MultipleSamplingPointsInstr : public SamplingPointsInstr {
  vector<SamplingPointsInstr*> pointGenerators;

public:
  MultipleSamplingPointsInstr(vector<SamplingPointsInstr*> pointGenerators);
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class StoreSamplingPointsInstr : public SamplingPointsInstr {
  SamplingTime time;

public:
  StoreSamplingPointsInstr(SamplingTime time);
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class LoopSamplingPointsInstr : public SamplingPointsInstr {
  SamplingTime time;

public:
  LoopSamplingPointsInstr(SamplingTime time);
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class FunctionSamplingPointsInstr : public SamplingPointsInstr {
  SamplingTime time;

public:
  FunctionSamplingPointsInstr(SamplingTime time);
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class FunctionCallSamplingPointsInstr : public SamplingPointsInstr {
  SamplingTime time;

public:
  FunctionCallSamplingPointsInstr(SamplingTime time);
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class BasicBlockSamplingPointsInstr : public SamplingPointsInstr {
  SamplingTime time;

public:
  BasicBlockSamplingPointsInstr(SamplingTime time);
  virtual vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class DataTraceInstr {
  AnalysisInstr* analysis;
  ScopeInstr* scope;
  SamplingPointsInstr* points;
  
  set<string> instrumentedFunctions;

  void install_recursive(struct instTarget& target, vector<BPatch_function*>& instrumentedFuncStack,
			 BPatch_function* currentFunction);
  
public:
  DataTraceInstr(AnalysisInstr* analysis, ScopeInstr* scope, SamplingPointsInstr* points);

  bool install(struct instTarget& target, std::string rootFuncName);
  void finishAnalysis(struct instTarget& target);
};

#endif // __TRACEAPIINSTR_H
