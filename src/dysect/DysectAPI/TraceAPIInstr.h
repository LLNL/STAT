
#ifndef __TRACEAPIINSTR_H
#define __TRACEAPIINSTR_H

#include "BPatch.h"
#include "BPatch_function.h"
#include "BPatch_point.h"

#include "ProcessSet.h"

#include "DysectAPI/TraceAPI.h"

#include <set>
#include <string>
#include <vector>

struct instTarget {
  BPatch_addressSpace* addrHandle;
  BPatch_image* appImage;
};

class AnalysisInstr {
public:
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function) = 0;
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint) = 0;
  virtual void finishAnalysis(struct instTarget& target) = 0;
  virtual bool pointAwareAnalysis() { return false; }
};

class CollectValuesInstr : public AnalysisInstr {
  CollectValues* original;

  std::string variableName;
  const int bufSize;
  const bool allValues;

  BPatch_function* collector;
  BPatch_variableExpr* variable;
  BPatch_variableExpr* buffer;
  BPatch_variableExpr* bufferIndex;
  BPatch_variableExpr* bufferSize;

public:
  CollectValuesInstr(CollectValues* original, std::string variableName, int bufSize = 2048, bool allValues = false);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual void finishAnalysis(struct instTarget& target);
};

class CountInvocationsInstr : public AnalysisInstr {
  CountInvocations* original;

  BPatch_function* collector;
  BPatch_variableExpr* counter;

public:
  CountInvocationsInstr(CountInvocations* original);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual void finishAnalysis(struct instTarget& target);
};

class BucketsInstr : public AnalysisInstr {
  Buckets* original;

  struct buckets {
    char* bitmap;
    int rangeStart;
    int rangeEnd;
    int count;
    int stepSize;
  };

  BPatch_function* collector;
  BPatch_variableExpr* variable;
  BPatch_variableExpr* bitmap;
  BPatch_variableExpr* procBkts;

  std::string variableName;
  struct buckets bkts;

 public:
  BucketsInstr(Buckets* original, std::string variableName, int rangeStart, int rangeEnd, int count, int stepSize);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual void finishAnalysis(struct instTarget& target);
};


class AverageInstr : public AnalysisInstr {
  Average* original;

  std::string variableName;

  BPatch_function* collector;
  BPatch_variableExpr* variable;
  BPatch_variableExpr* lastVal;
  BPatch_variableExpr* average;
  BPatch_variableExpr* count;

public:
  AverageInstr(Average* original, std::string variableName);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual void finishAnalysis(struct instTarget& target);
};

class InvariantGeneratorInstr : public AnalysisInstr {
  InvariantGenerator* original;
  std::string variableName;
  BPatch_function* collector;
  BPatch_variableExpr* variable;
  BPatch_variableExpr* orgVal;
  BPatch_variableExpr* modifications;
  BPatch_variableExpr* initialized;

public:
  InvariantGeneratorInstr(InvariantGenerator* original, std::string variableName);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual void finishAnalysis(struct instTarget& target);
};

class ExtractFeaturesInstr : public AnalysisInstr {
  ExtractFeatures* original;

  std::string variableName;
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
  ExtractFeaturesInstr(ExtractFeatures* original, std::string variableName);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual void finishAnalysis(struct instTarget& target);
};

class PrintChangesInstr : public AnalysisInstr {
  PrintChanges* original;

  std::string variableName;
  BPatch_variableExpr* variable;
  BPatch_function* logAddrFunc;
  BPatch_variableExpr* lastVal;

public:
  PrintChangesInstr(PrintChanges* original, std::string variableName);
  virtual void finishAnalysis(struct instTarget& target);
  virtual bool prepareInstrumentedFunction(struct instTarget& target, BPatch_function* function);
  virtual BPatch_snippet* getInstrumentationSnippet(struct instTarget& target, BPatch_point* instrumentationPoint);
  virtual bool pointAwareAnalysis() { return true; }
};

class ScopeInstr {
public:
  enum ShouldInstrument {
    Instrument,
    Skip,
    StopSearch
  };

  virtual ShouldInstrument shouldInstrument(std::vector<BPatch_function*>& instrumentedFunctions,
				BPatch_function* function) = 0;
  virtual bool limitToCallPath();
};

class FunctionScopeInstr : public ScopeInstr {
  int maxCallPath;

public:
  FunctionScopeInstr(int maxCallPath);
  ShouldInstrument shouldInstrument(std::vector<BPatch_function*>& instrumentedFunctions, BPatch_function* function);
};

class CallPathScopeInstr : public ScopeInstr {
  std::vector<std::string> callPath;

public:
  CallPathScopeInstr(std::vector<std::string> callPath);
  virtual ShouldInstrument shouldInstrument(std::vector<BPatch_function*>& instrumentedFunctions, BPatch_function* function);
};

class CalledFunctionInstr : public ScopeInstr {
  std::string fname;

 public:
  CalledFunctionInstr(std::string fname);
  virtual ShouldInstrument shouldInstrument(std::vector<BPatch_function*>& instrumentedFunctions, BPatch_function* function);
  virtual bool limitToCallPath();
};

class SamplingPointsInstr {
public:
  virtual std::vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function) = 0;

  enum SamplingTime {
    beginning, end, before, after
  };
};

class MultipleSamplingPointsInstr : public SamplingPointsInstr {
  std::vector<SamplingPointsInstr*> pointGenerators;

public:
  MultipleSamplingPointsInstr(std::vector<SamplingPointsInstr*> pointGenerators);
  virtual std::vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class StoreSamplingPointsInstr : public SamplingPointsInstr {
  SamplingTime time;

public:
  StoreSamplingPointsInstr(SamplingTime time);
  virtual std::vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class LoopSamplingPointsInstr : public SamplingPointsInstr {
  SamplingTime time;

public:
  LoopSamplingPointsInstr(SamplingTime time);
  virtual std::vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class FunctionSamplingPointsInstr : public SamplingPointsInstr {
  SamplingTime time;

public:
  FunctionSamplingPointsInstr(SamplingTime time);
  virtual std::vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class FunctionCallSamplingPointsInstr : public SamplingPointsInstr {
  SamplingTime time;

public:
  FunctionCallSamplingPointsInstr(SamplingTime time);
  virtual std::vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class BasicBlockSamplingPointsInstr : public SamplingPointsInstr {
  SamplingTime time;

public:
  BasicBlockSamplingPointsInstr(SamplingTime time);
  virtual std::vector<BPatch_point*> getInstrumentationPoints(struct instTarget& target, BPatch_function* function);
};

class DataTraceInstr {
  AnalysisInstr* analysis;
  ScopeInstr* scope;
  SamplingPointsInstr* points;

  std::set<std::string> instrumentedFunctions;

  void install_recursive(struct instTarget& target, std::vector<BPatch_function*>& instrumentedFuncStack,
			 BPatch_function* currentFunction);

  void loadAnalyticsLibrary(struct instTarget& target);

public:
  DataTraceInstr(AnalysisInstr* analysis, ScopeInstr* scope, SamplingPointsInstr* points);

  bool install(struct instTarget& target, std::string rootFuncName);
  void finishAnalysis(struct instTarget& target);
};

class TraceAPIInstr {
  static std::map<BPatch_addressSpace*, BPatch_object*> analyticsLib;

  static BPatch_object* loadAnalyticsLibrary(struct instTarget& target);

public:
  static BPatch_object* getAnalyticsLib(struct instTarget& target);
};

#endif // __TRACEAPIINSTR_H
