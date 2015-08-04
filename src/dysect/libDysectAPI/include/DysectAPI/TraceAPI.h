
#ifndef __TRACEAPI_H
#define __TRACEAPI_H

class Analysis {
public:
  static Analysis* printChanges(string variableName);
  static Analysis* extractFeatures(string variableName);
  static Analysis* generateInvariant(string variableName);
  static Analysis* collectValues(string variableName);

  virtual ~Analysis() {}
};

class CollectValues : public Analysis {
  friend class DataTrace;
  
  string variableName;
  const int bufSize;
  const bool allValues;
  
public:
  CollectValues(string variableName, int bufSize = 2048, bool allValues = false);
};

class InvariantGenerator : public Analysis {
  friend class DataTrace;
  
  string variableName;
  
public:
  InvariantGenerator(string variableName);
};

class ExtractFeatures : public Analysis {
  friend class DataTrace;
  
  string variableName;

public:
  ExtractFeatures(string variableName);
};

class PrintChanges : public Analysis {
  friend class DataTrace;
  
  string variableName;
  
public:
  PrintChanges(string variableName);
};

class Scope {
public:
  static Scope* singleFunction();
  static Scope* reachableFunctions(int calls = numeric_limits<int>::max());
  static Scope* callPath(string f1 = "", string f2 = "", string f3 = "",
			 string f4 = "", string f5 = "", string f6 = "",
			 string f7 = "", string f8 = "", string f9 = "");

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
  
  vector<string> callPath;
  
public:
  CallPathScope(vector<string> callPath);
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
  
  vector<SamplingPoints*> pointGenerators;

public:
  MultipleSamplingPoints(vector<SamplingPoints*> pointGenerators);
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
  static bool instrumentProcess(Dyninst::ProcControlAPI::Process::const_ptr proc, DataTrace* trace);
  static bool finishAnalysis(Dyninst::ProcControlAPI::Process::const_ptr proc, DataTrace* trace);
};


#endif
