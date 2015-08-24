
#include <LibDysectAPI.h>

#include <DysectAPI/DysectAPIProcessMgr.h>
#include <DysectAPI/TraceAPI.h>
#include <DysectAPI/Err.h>

using namespace std;
using namespace DysectAPI;

/************ Analysis ************/
void Analysis::getAggregateRefs(std::vector<DysectAPI::AggregateFunction*>& aggs) {
  
}

bool Analysis::evaluateAggregate(DysectAPI::AggregateFunction* aggregate) {
  return true;
}

bool Analysis::usesGlobalResult() {
  return false;
}

bool Analysis::formatGlobalResult(char*& packet, int& size) {
  size = 0;

  return false;
}

void Analysis::processGlobalResult(char* packet, int size) {

}

CollectValues::CollectValues(string variableName, int bufSize, bool allValues)
  : variableName(variableName), bufSize(bufSize), allValues(allValues) {

}

void CollectValues::getAggregateRefs(std::vector<DysectAPI::AggregateFunction*>& aggs) {
  aggs.push_back(&aggregator);
}

bool CollectValues::evaluateAggregate(AggregateFunction* aggregate) {
  CollectValuesAgg* agg = dynamic_cast<CollectValuesAgg*>(aggregate);
  if (agg == 0) {
    return false;
  }

  // Copy the analysis result into our aggregate 
  aggregator.copy(agg);
  
  string desc;
  aggregator.getStr(desc);
  DYSECTINFO(true, "The variable %s took the values %s", variableName.c_str(), desc.c_str());

  return true;
}

bool CollectValues::usesGlobalResult() {
  return true;
}

bool CollectValues::formatGlobalResult(char*& packet, int& size) {
  size = aggregator.getSize();

  packet = (char*)malloc(size);
  aggregator.writeSubpacket(packet);

  return true;
}

void CollectValues::processGlobalResult(char* packet, int size) {
  globalResult.readSubpacket(packet);
}

DysectAPI::CollectValuesAgg* CollectValues::getAggregator() {
  return &aggregator;
}

DysectAPI::CollectValuesAgg* CollectValues::getGlobalResult() {
  return &globalResult;
}

DysectAPI::CollectValuesIncludes* CollectValues::includes(int value) {
  return new DysectAPI::CollectValuesIncludes(this, value);
}


CountInvocations::CountInvocations(bool synchronize) : synchronize(synchronize) {
  
}

void CountInvocations::getAggregateRefs(std::vector<DysectAPI::AggregateFunction*>& aggs) {
  aggs.push_back(&aggregator);
}

bool CountInvocations::evaluateAggregate(AggregateFunction* aggregate) {
  CountInvocationsAgg* agg = dynamic_cast<CountInvocationsAgg*>(aggregate);
  if (agg == 0) {
    return false;
  }

  // Copy the analysis result into our aggregate 
  aggregator.copy(agg);
  
  DYSECTINFO(true, "The instrumented snippet was invoked %d times", aggregator.getCounter());

  return true;
}

bool CountInvocations::usesGlobalResult() {
  return synchronize;
}

bool CountInvocations::formatGlobalResult(char*& packet, int& size) {
  size = aggregator.getSize();

  packet = (char*)malloc(size);
  aggregator.writeSubpacket(packet);

  return true;
}

void CountInvocations::processGlobalResult(char* packet, int size) {
  globalResult.readSubpacket(packet);

  DYSECTVERBOSE(true, "Obtained global result: %d invocations in total.", globalResult.getCounter());
}

DysectAPI::CountInvocationsAgg* CountInvocations::getAggregator() {
  return &aggregator;
}

DysectAPI::CountInvocationsAgg* CountInvocations::getGlobalResult() {
  return &globalResult;
}


Buckets::Buckets(string variableName, int rangeStart, int rangeEndIn, int count)
  : variableName(variableName),
    rangeStart(rangeStart),
    rangeEnd(rangeEndIn - ((rangeEndIn - rangeStart) % count)), /* Fix end range if things doesnt add up */
    count(count),
    stepSize((rangeEnd - rangeStart) / count),
    aggregator(rangeStart, rangeEnd, stepSize, count),
    globalResult(rangeStart, rangeEnd, stepSize, count) {
  DYSECTVERBOSE(true, "The range end %d was changed to %d", rangeEndIn, rangeEnd); 
}

void Buckets::getAggregateRefs(std::vector<DysectAPI::AggregateFunction*>& aggs) {
  aggs.push_back(&aggregator);
}

bool Buckets::evaluateAggregate(AggregateFunction* aggregate) {
  RankBucketAgg* agg = dynamic_cast<RankBucketAgg*>(aggregate);
  if (agg == 0) {
    return false;
  }

  // Copy the analysis result into our aggregate 
  aggregator.copy(agg);

  string bucketStr;
  aggregator.getStr(bucketStr);
  DYSECTINFO(true, "Buckets of variable %s:\n%s", variableName.c_str(), bucketStr.c_str());

  return true;
}

bool Buckets::usesGlobalResult() {
  return false;
}

bool Buckets::formatGlobalResult(char*& packet, int& size) {
  size = aggregator.getSize();

  packet = (char*)malloc(size);
  aggregator.writeSubpacket(packet);

  return true;
}

void Buckets::processGlobalResult(char* packet, int size) {
  DYSECTWARN(false, "Analysis does not use global result!");
}

DysectAPI::RankBucketAgg* Buckets::getAggregator() {
  return &aggregator;
}

DysectAPI::RankBucketAgg* Buckets::getGlobalResult() {
  return &globalResult;
}


Average::Average(string variableName)
  : variableName(variableName) {

}

void Average::getAggregateRefs(std::vector<DysectAPI::AggregateFunction*>& aggs) {
  aggs.push_back(&aggregator);
}

bool Average::evaluateAggregate(AggregateFunction* aggregate) {
  AverageAgg* agg = dynamic_cast<AverageAgg*>(aggregate);
  if (agg == 0) {
    return false;
  }

  // Copy the analysis result into our aggregate 
  aggregator.copy(agg);
  
  string desc;
  aggregator.getStr(desc);
  DYSECTINFO(true, "The observed average of %s across nodes is %s", variableName.c_str(), desc.c_str());

  return true;
}

bool Average::usesGlobalResult() {
  return false;
}

bool Average::formatGlobalResult(char*& packet, int& size) {
  size = aggregator.getSize();

  packet = (char*)malloc(size);
  aggregator.writeSubpacket(packet);

  return true;
}

void Average::processGlobalResult(char* packet, int size) {
  globalResult.readSubpacket(packet);
}

DysectAPI::AverageAgg* Average::getAggregator() {
  return &aggregator;
}

DysectAPI::AverageAgg* Average::getGlobalResult() {
  return &globalResult;
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

Analysis* Analysis::collectValues(string variableName) {
  return new CollectValues(variableName);
}

Analysis* Analysis::countInvocations(bool synchronize) {
  return new CountInvocations(synchronize);
}

Analysis* Analysis::buckets(std::string variableName, int rangeStart, int rangeEnd, int count) {
  return new Buckets(variableName, rangeStart, rangeEnd, count);
}

Analysis* Analysis::average(std::string variableName) {
  return new Average(variableName);
}

/************ Scope ************/
FunctionScope::FunctionScope(int maxCallPath) : maxCallPath(maxCallPath) {
    
}
  
CallPathScope::CallPathScope(vector<string> callPath) : callPath(callPath) {

}

CalledFunction::CalledFunction(string fname) : fname(fname) {

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

Scope* Scope::calledFunction(std::string fname) {
  return new CalledFunction(fname);
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
  analysis->getAggregateRefs(aggregates);
  
  createInstrumentor();
}

std::vector<DysectAPI::AggregateFunction*>* DataTrace::getAggregates() {
  return &aggregates;
}

bool DataTrace::evaluateAggregate(DysectAPI::AggregateFunction* aggregate) {
  analysis->evaluateAggregate(aggregate);
}

bool DataTrace::usesGlobalResult() {
  return analysis->usesGlobalResult();
}

bool DataTrace::formatGlobalResult(char*& packet, int& size) {
  return analysis->formatGlobalResult(packet, size);
}

void DataTrace::processGlobalResult(char* packet, int size) {
  analysis->processGlobalResult(packet, size);
}

/************ TraceAPI ************/
multimap<Dyninst::ProcControlAPI::Process::const_ptr, struct TraceAPI::pendingInst> TraceAPI::pendingInstrumentations;
multimap<Dyninst::ProcControlAPI::Process::const_ptr, DataTrace*> TraceAPI::pendingAnalysis;
multimap<DataTrace*, Dyninst::ProcControlAPI::Process::const_ptr> TraceAPI::pendingGlobalRes;

