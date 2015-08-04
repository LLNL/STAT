
#ifndef __TRACEAPI_H
#define __TRACEAPI_H

#include <string>
#include <limits>
#include <vector>

class Analysis {
public:
  static Analysis* printChanges(std::string variableName);
  static Analysis* extractFeatures(std::string variableName);
  static Analysis* generateInvariant(std::string variableName);
  static Analysis* collectValues(std::string variableName);

  virtual ~Analysis() {}
};

class CollectValues : public Analysis {
  friend class DataTrace;
  
  std::string variableName;
  const int bufSize;
  const bool allValues;
  
public:
  CollectValues(std::string variableName, int bufSize = 2048, bool allValues = false);
};

class InvariantGenerator : public Analysis {
  friend class DataTrace;
  
  std::string variableName;
  
public:
  InvariantGenerator(std::string variableName);
};

class ExtractFeatures : public Analysis {
  friend class DataTrace;
  
  std::string variableName;

public:
  ExtractFeatures(std::string variableName);
};

class PrintChanges : public Analysis {
  friend class DataTrace;
  
  std::string variableName;
  
public:
  PrintChanges(std::string variableName);
};

class Scope {
public:
  static Scope* singleFunction();
  static Scope* reachableFunctions(int calls = std::numeric_limits<int>::max());
  static Scope* callPath(std::string f1 = "", std::string f2 = "", std::string f3 = "",
			 std::string f4 = "", std::string f5 = "", std::string f6 = "",
			 std::string f7 = "", std::string f8 = "", std::string f9 = "");

  virtual ~Scope() {}
};

class FunctionScope : public Scope {
  friend class DataTrace;
  
  int maxCallPath;
  
public:
  FunctionScope(int maxCallPath);
};

class CallPathScope : public Scope {
  friend class DataTrace;
  
  std::vector<std::string> callPath;
  
public:
  CallPathScope(std::vector<std::string> callPath);
};

class SamplingPoints {
public:
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

  virtual ~SamplingPoints() {}
};

class MultipleSamplingPoints : public SamplingPoints {
  friend class DataTrace;
  
  std::vector<SamplingPoints*> pointGenerators;

public:
  MultipleSamplingPoints(std::
			 vector<SamplingPoints*> pointGenerators);
};

class StoreSamplingPoints : public SamplingPoints {
  friend class DataTrace;
  
  SamplingTime time;

public:
  StoreSamplingPoints(SamplingTime time);
};

class LoopSamplingPoints : public SamplingPoints {
  friend class DataTrace;
  
  SamplingTime time;

public:
  LoopSamplingPoints(SamplingTime time);
};

class FunctionSamplingPoints : public SamplingPoints {
  friend class DataTrace;
  
  SamplingTime time;

public:
  FunctionSamplingPoints(SamplingTime time);
};

class FunctionCallSamplingPoints : public SamplingPoints {
  friend class DataTrace;
  
  SamplingTime time;

public:
  FunctionCallSamplingPoints(SamplingTime time);
};

class BasicBlockSamplingPoints : public SamplingPoints {
  friend class DataTrace;
  
  SamplingTime time;

public:
  BasicBlockSamplingPoints(SamplingTime time);
};

class DataTrace {
  Analysis* analysis;
  Scope* scope;
  SamplingPoints* points;

  // Pointer to the DataTraceInstr objet used to perform instrumentation
  //  The front ent cannot be linked against DyninstAPI which DataTraceInstr
  //  is, therefore a void pointer is used. 
  void* instrumentor;

  void createInstrumentor();
  void* convertSamplingPoints(SamplingPoints* points);
  
public:
  DataTrace(Analysis* analysis, Scope* scope, SamplingPoints* points);
};

class TraceAPI {
 public:
#ifdef PORT_LATER
  static bool instrumentProcess(Dyninst::ProcControlAPI::Process::const_ptr proc, DataTrace* trace);
  static bool finishAnalysis(Dyninst::ProcControlAPI::Process::const_ptr proc, DataTrace* trace);
#endif // PORT_LATER
};


#endif
