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

using namespace std;
using namespace DysectAPI;

/* Conversion functions */
SamplingPointsInstr::SamplingTime convertTime(SamplingPoints::SamplingTime time);

void DataTrace::createInstrumentor() {
  AnalysisInstr* analysisInstr = 0;
  ScopeInstr* scopeInstr = 0;
  SamplingPointsInstr* pointsInstr = 0;
  DataTraceInstr* dataTraceInstr = 0;

  /* Create analysis */
  {
    CollectValues* colVal;
    InvariantGenerator* invGen;
    ExtractFeatures* extFeat;
    PrintChanges* prtChg;

    if (colVal = dynamic_cast<CollectValues*>(analysis)) {
      analysisInstr = new CollectValuesInstr(colVal->variableName);
    } else if (invGen = dynamic_cast<InvariantGenerator*>(analysis)) {
      analysisInstr = new InvariantGeneratorInstr(invGen->variableName);
    } else if (extFeat = dynamic_cast<ExtractFeatures*>(analysis)) {
      analysisInstr = new ExtractFeaturesInstr(extFeat->variableName);
    } else if (prtChg = dynamic_cast<PrintChanges*>(analysis)) {
      analysisInstr = new PrintChangesInstr(prtChg->variableName);
    } else {
      assert(!"Unknown analysis!");
    }
  }

  /* Create scope */
  {
    FunctionScope* functionScp;
    CallPathScope* callPathScp;

    if (functionScp = dynamic_cast<FunctionScope*>(scope)) {
      scopeInstr = new FunctionScopeInstr(functionScp->maxCallPath);
    } else if (callPathScp = dynamic_cast<CallPathScope*>(scope)) {
      scopeInstr = new CallPathScopeInstr(callPathScp->callPath);
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
  
  instrumentor = (void*)dataTraceInstr;
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


bool DataTrace::instrumentProcess(Dyninst::ProcControlAPI::Process::const_ptr proc) {
  ProcDebug* pDebug;
  bool wasRunning = false;
  Stackwalker::Walker* walker = ProcMap::get()->getWalker(proc);
  
  if (proc->allThreadsRunning()) {
    wasRunning = true;
    pDebug = dynamic_cast<ProcDebug*>(walker->getProcessState());
    //    pDebug->pause();
  }
  
  vector<Stackwalker::Frame> stackWalk;

  if (!walker->walkStack(stackWalk)) {
    if (wasRunning) {
      pDebug->resume();
    }
		    
    return false;
  }

  string curFuncName;
  stackWalk[0].getName(curFuncName);

  instTarget target;
  BPatch_process* dyninst_proc = ProcMap::get()->getDyninstProcess(proc);
  target.addrHandle = dynamic_cast<BPatch_addressSpace*>(dyninst_proc);
  target.appImage = target.addrHandle->getImage();

  if (dyninst_proc->stopExecution()) {
    DYSECTVERBOSE(false, "Could not stop process!");
  }
  
  DataTraceInstr* instr = (DataTraceInstr*)instrumentor;
  bool res = instr->install(target, curFuncName);

  if (dyninst_proc->continueExecution()) {
    DYSECTVERBOSE(false, "Could not continue process!");
  }

  if (wasRunning) {
    pDebug->resume();
  }

  return res;
}

void DataTrace::finishAnalysis(Dyninst::ProcControlAPI::Process::const_ptr proc) {
  instTarget target;
  BPatch_process* dyninst_proc = ProcMap::get()->getDyninstProcess(proc);
  target.addrHandle = dynamic_cast<BPatch_addressSpace*>(dyninst_proc);
  target.appImage = target.addrHandle->getImage();

  dyninst_proc->stopExecution();
  
  DataTraceInstr* instr = (DataTraceInstr*)instrumentor;
  instr->finishAnalysis(target);

  dyninst_proc->continueExecution();
}
