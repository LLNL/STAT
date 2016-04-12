/*
Copyright (c) 2013-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Jesper Puge Nielsen, Niklas Nielsen, Gregory Lee [lee218@llnl.gov], Dong Ahn.
LLNL-CODE-645136.
All rights reserved.

This file is part of DysectAPI. For details, see https://github.com/lee218llnl/DysectAPI. Please also read dysect/LICENSE

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <LibDysectAPI.h>
#include <DysectAPI/Err.h>
#include <DysectAPI/TraceAPI.h>
#include <DysectAPI/TraceAPIInstr.h>
#include <DysectAPI/ProcMap.h>
#include <DysectAPI/DysectAPIProcessMgr.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

/* Conversion functions */
SamplingPointsInstr::SamplingTime convertTime(SamplingPoints::SamplingTime time);

void* DataTrace::createInstrumentor() {
  AnalysisInstr* analysisInstr = 0;
  ScopeInstr* scopeInstr = 0;
  SamplingPointsInstr* pointsInstr = 0;
  DataTraceInstr* dataTraceInstr = 0;

  /* Create analysis */
  {
    CollectValues* colVal;
    CountInvocations* cntInv;
    InvariantGenerator* invGen;
    ExtractFeatures* extFeat;
    PrintChanges* prtChg;
    Buckets* bkts;
    Average* avg;
    
    if (colVal = dynamic_cast<CollectValues*>(analysis)) {
      analysisInstr = new CollectValuesInstr(colVal, colVal->variableName, colVal->bufSize, colVal->allValues);
    } else if (cntInv = dynamic_cast<CountInvocations*>(analysis)) {
      analysisInstr = new CountInvocationsInstr(cntInv);
    } else if (invGen = dynamic_cast<InvariantGenerator*>(analysis)) {
      analysisInstr = new InvariantGeneratorInstr(invGen, invGen->variableName);
    } else if (extFeat = dynamic_cast<ExtractFeatures*>(analysis)) {
      analysisInstr = new ExtractFeaturesInstr(extFeat, extFeat->variableName);
    } else if (prtChg = dynamic_cast<PrintChanges*>(analysis)) {
      analysisInstr = new PrintChangesInstr(prtChg, prtChg->variableName);
    } else if (bkts = dynamic_cast<Buckets*>(analysis)) {
      analysisInstr = new BucketsInstr(bkts, bkts->variableName, bkts->rangeStart,
                                       bkts->rangeEnd, bkts->count, bkts->stepSize);
    } else if (avg = dynamic_cast<Average*>(analysis)) {
      analysisInstr = new AverageInstr(avg, avg->variableName);
    } else {
      assert(!"Unknown analysis!");
    }
  }

  /* Create scope */
  {
    FunctionScope* functionScp;
    CallPathScope* callPathScp;
    CalledFunction* calledFuncScp;

    if (functionScp = dynamic_cast<FunctionScope*>(scope)) {
      scopeInstr = new FunctionScopeInstr(functionScp->maxCallPath);
    } else if (callPathScp = dynamic_cast<CallPathScope*>(scope)) {
      scopeInstr = new CallPathScopeInstr(callPathScp->callPath);
    } else if (calledFuncScp = dynamic_cast<CalledFunction*>(scope)) {
      scopeInstr = new CalledFunctionInstr(calledFuncScp->fname);
    } else {
      assert(!"Unkown scope!");
    }
  }

  /* Create sampling points */
  pointsInstr = (SamplingPointsInstr*)convertSamplingPoints(points);

  /* Security checks */
  assert(analysisInstr);
  assert(scopeInstr);
  assert(pointsInstr);

  /* Create the new trace and save it */
  dataTraceInstr = new DataTraceInstr(analysisInstr, scopeInstr, pointsInstr);
  
  return (void*)dataTraceInstr;
}

/* The function must be a member of DataTrace to access private members
    of the SamplingPoints classes. It must return a void pointer as
    SamplingPointsInstr cannot be included in DataTrace.h */
void* DataTrace::convertSamplingPoints(SamplingPoints* points) {
  SamplingPointsInstr* pointsInstr = 0;
  
  MultipleSamplingPoints* multiPts;
  StoreSamplingPoints* storePts;
  LoopSamplingPoints* loopPts;
  FunctionSamplingPoints* funcPts;
  BasicBlockSamplingPoints* blockPts;
  FunctionCallSamplingPoints* funcCallPts;
  
  if (multiPts = dynamic_cast<MultipleSamplingPoints*>(points)) {
    vector<SamplingPointsInstr*> instrPoints;

    for (vector<SamplingPoints*>::iterator it = multiPts->pointGenerators.begin();
         it != multiPts->pointGenerators.end(); ++it) {
      instrPoints.push_back((SamplingPointsInstr*)convertSamplingPoints(*it));
    }
    
    pointsInstr = new MultipleSamplingPointsInstr(instrPoints);
  } else if (storePts = dynamic_cast<StoreSamplingPoints*>(points)) {
    pointsInstr = new StoreSamplingPointsInstr(convertTime(storePts->time));
  } else if (loopPts = dynamic_cast<LoopSamplingPoints*>(points)) {
    pointsInstr = new LoopSamplingPointsInstr(convertTime(loopPts->time));
  } else if (funcPts = dynamic_cast<FunctionSamplingPoints*>(points)) {
    pointsInstr = new FunctionSamplingPointsInstr(convertTime(funcPts->time));
  } else if (blockPts = dynamic_cast<BasicBlockSamplingPoints*>(points)) {
    pointsInstr = new BasicBlockSamplingPointsInstr(convertTime(blockPts->time));
  } else if (funcCallPts = dynamic_cast<FunctionCallSamplingPoints*>(points)) {
    pointsInstr = new FunctionCallSamplingPointsInstr(convertTime(funcCallPts->time));
  } else {
    assert(!"Unknown sampling point!");
  }

  return (void*)pointsInstr;
}

SamplingPointsInstr::SamplingTime convertTime(SamplingPoints::SamplingTime time) {
  switch (time) {
  case SamplingPoints::beginning:
    return SamplingPointsInstr::beginning;
  case SamplingPoints::end:
    return SamplingPointsInstr::end;
  case SamplingPoints::before:
    return SamplingPointsInstr::before;
  case SamplingPoints::after:
    return SamplingPointsInstr::after;
  default:
    assert(!"Unknown sampling time!");
  }
}


bool DataTrace::instrumentProcess(Dyninst::ProcControlAPI::Process::const_ptr proc, string rootFunc) {
  instTarget target;
  BPatch_process* dyninst_proc = ProcMap::get()->getDyninstProcess(proc);
  target.addrHandle = dynamic_cast<BPatch_addressSpace*>(dyninst_proc);
  target.appImage = target.addrHandle->getImage();

  DataTraceInstr* instr;
  int pid = dyninst_proc->getPid();
  
  map<int, void*>::iterator it = instrumentors.find(pid);
  if (it == instrumentors.end()) {
    DYSECTVERBOSE(true, "Creating instrumentor for process %d", pid);
    
    instr = (DataTraceInstr*)createInstrumentor();
    instrumentors[pid] = (void*)instr;
  } else {
    instr = (DataTraceInstr*)(it->second);
  }
  
  bool res = instr->install(target, rootFunc);

  return res;
}

void DataTrace::finishAnalysis(Dyninst::ProcControlAPI::Process::const_ptr proc) {
  instTarget target;
  BPatch_process* dyninst_proc = ProcMap::get()->getDyninstProcess(proc);
  target.addrHandle = dynamic_cast<BPatch_addressSpace*>(dyninst_proc);
  target.appImage = target.addrHandle->getImage();
  
  DataTraceInstr* instr;
  int pid = dyninst_proc->getPid();
  
  map<int, void*>::iterator it = instrumentors.find(pid);
  if (it == instrumentors.end()) {
    DYSECTFATAL(false, "Could not find instrumentor for process %d", pid);
    
    return;
  } else {
    instr = (DataTraceInstr*)(it->second);
  }
  
  instr->finishAnalysis(target);
}

void TraceAPI::addPendingInstrumentation(Dyninst::ProcControlAPI::Process::const_ptr proc, DataTrace* trace, string rootFunc) {
  ProcessMgr::waitFor(ProcWait::instrumentation, proc);

  struct pendingInst pInst;
  pInst.trace = trace;
  pInst.rootFunc = rootFunc;
  
  pendingInstrumentations.insert(pair<Dyninst::ProcControlAPI::Process::const_ptr, struct pendingInst>(proc, pInst));
}

void TraceAPI::performPendingInstrumentations() {
  if (pendingInstrumentations.size() == 0) {
    return;
  }

  set<Process::const_ptr> instrumented_procs;
  
  multimap<Dyninst::ProcControlAPI::Process::const_ptr, struct pendingInst>::iterator it;
  for (it = pendingInstrumentations.begin(); it != pendingInstrumentations.end(); ++it) {
    Dyninst::ProcControlAPI::Process::const_ptr process = it->first;
    DataTrace* trace = it->second.trace;
    string rootFunc = it->second.rootFunc;

    ProcessMgr::handled(ProcWait::instrumentation, process);

    trace->instrumentProcess(process, rootFunc);
    instrumented_procs.insert(process);
  }

  ProcessSet::ptr instrumented_procset = ProcessSet::newProcessSet(instrumented_procs);
  ProcessMgr::continueProcsIfReady(instrumented_procset);

  pendingInstrumentations.clear();
}

void TraceAPI::addPendingAnalysis(Dyninst::ProcControlAPI::Process::const_ptr proc, DataTrace* trace) {
  ProcessMgr::waitFor(ProcWait::analysis, proc);
  pendingAnalysis.insert(pair<Dyninst::ProcControlAPI::Process::const_ptr, DataTrace*>(proc, trace));
  
  if (trace->usesGlobalResult()) {
    ProcessMgr::waitFor(ProcWait::globalResult, proc);
    pendingGlobalRes.insert(pair<DataTrace*, Dyninst::ProcControlAPI::Process::const_ptr>(trace, proc));
  }
}

void TraceAPI::performPendingAnalysis() {
  if (pendingAnalysis.size() == 0) {
    return;
  }

  set<Process::const_ptr> analyzed_procs;
  
  multimap<Dyninst::ProcControlAPI::Process::const_ptr, DataTrace*>::iterator it;
  for (it = pendingAnalysis.begin(); it != pendingAnalysis.end(); ++it) {
    Dyninst::ProcControlAPI::Process::const_ptr process = it->first;
    DataTrace* trace = it->second;

    ProcessMgr::handled(ProcWait::analysis, process);

    trace->finishAnalysis(process);
    analyzed_procs.insert(process);
  }

  ProcessSet::ptr analyzed_procset = ProcessSet::newProcessSet(analyzed_procs);
  ProcessMgr::continueProcsIfReady(analyzed_procset);

  pendingAnalysis.clear();
}

void TraceAPI::processedGlobalRes(DataTrace* trace) {
  typedef multimap<DataTrace*, Dyninst::ProcControlAPI::Process::const_ptr>::iterator mapIter;

  set<Process::const_ptr> processedProcs;
  
  pair<mapIter, mapIter> range = pendingGlobalRes.equal_range(trace);
  for (mapIter it = range.first; it != range.second; ++it) {
    Dyninst::ProcControlAPI::Process::const_ptr process = it->second;
    
    ProcessMgr::handled(ProcWait::globalResult, process);

    processedProcs.insert(process);
  }
  pendingGlobalRes.erase(trace);

  ProcessSet::ptr processedProcset = ProcessSet::newProcessSet(processedProcs);
  ProcessMgr::continueProcsIfReady(processedProcset);

}
