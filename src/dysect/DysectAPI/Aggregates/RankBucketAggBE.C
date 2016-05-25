/*
Copyright (c) 2013-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Jesper Puge Neilsen, Niklas Nielsen, Gregory Lee [lee218@llnl.gov], Dong Ahn.
LLNL-CODE-645136.
All rights reserved.

This file is part of DysectAPI. For details, see https://github.com/lee218llnl/DysectAPI. Please also read dysect/LICENSE

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

bool RankBucketAgg::collect(void* process, void *thread) {
  Process::const_ptr process_ptr = *(Process::const_ptr*)process;
  Thread::const_ptr thread_ptr = *(Thread::const_ptr*)thread;

  if(!process_ptr) {
    return DYSECTVERBOSE(false, "Process object not available");
  }

  // Get the rank of the process
  Probe *owner = getOwner();
  if (!owner) {
    return DYSECTVERBOSE(false, "Could not find Probe owner");
  }

  Domain *dom = owner->getDomain();
  if (!dom) {
    return DYSECTVERBOSE(false, "Could not find Domain");
  }

  std::map<int, Dyninst::ProcControlAPI::Process::ptr> *mpiRankToProcessMap;
  mpiRankToProcessMap = dom->getMpiRankToProcessMap();
  if (!mpiRankToProcessMap) {
    return DYSECTVERBOSE(false, "Could not find MPI rank map");
  }

  int rank = -1;
  std::map<int, Dyninst::ProcControlAPI::Process::ptr>::iterator iter;
  for (iter = mpiRankToProcessMap->begin(); iter != mpiRankToProcessMap->end(); iter++) {
    if (iter->second == process_ptr) {
      rank = iter->first;
      break;
    }
  }

  if (rank == -1) {
    return DYSECTVERBOSE(false, "Failed to determine Rank");
  }

  // Read the value of the variable
  ProcDebug* pDebug;
  Walker* proc = (Walker*)process_ptr->getData();

  if(!proc) {
    return DYSECTVERBOSE(false, "Could not get walker from process");
  }

  bool wasRunning;
  if (process_ptr->allThreadsRunning()) {
    wasRunning = true;

    pDebug = dynamic_cast<ProcDebug *>(proc->getProcessState());

    if (pDebug == NULL) {
      return DYSECTWARN(false, "Failed to dynamic_cast ProcDebug pointer");
    }

    if (pDebug->isTerminated()) {
      return DYSECTWARN(false, "Process is terminated");
    }

    if (pDebug->pause() == false) {
      return DYSECTWARN(false, "Failed to pause process");
    }
  }

  DataRef ref(Value::floatType, variableName.c_str());

  TargetVar var(variableName.c_str());
  ConditionResult condRes;
  Value val;

  DysectErrorCode readStatus = var.getValue(condRes, val, process_ptr, thread_ptr->getTID());

  if (wasRunning == true) {
    if (pDebug->resume() == false) {
      return DYSECTWARN(false, "Failed to resume process");
    }
  }

  if (readStatus != OK) {
    return DYSECTWARN(false, "Could not read value from reference");
  }

  if (condRes != Resolved) {
    return DYSECTINFO(false, "Value not available - could be optimized out!");
  }

  if (variableType == Value::noType) {
    variableType = val.getType();
  }

  int bucket = getBucketFromValue(val);
  buckets[bucket]->addRank(rank);

  string valStr;
  val.getStr(valStr);
  DYSECTVERBOSE(true, "Rank %d read the value %s and will place it in bucket %d", rank, valStr.c_str(), bucket);

  return true;
}

