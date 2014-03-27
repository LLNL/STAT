/*
Copyright (c) 2013-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Niklas Nielsen, Gregory Lee [lee218@llnl.gov], Dong Ahn.
LLNL-CODE-645136.
All rights reserved.

This file is part of DysectAPI. For details, see https://github.com/lee218llnl/DysectAPI. Please also read dysect/LICENSE

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License (as published by the Free Software Foundation) version 2.1 dated February 1999.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <DysectAPI.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

bool Group::lookupAttached() {
  set<pair<mpi_rank_t, mpi_rank_t> >::iterator intervalIter;
  pair<mpi_rank_t, mpi_rank_t> interval;
  int start, end;

  set<mpi_rank_t> allMPIRanks;

  for(int i = 0; i < processTableSize; i++) {
    allMPIRanks.insert(processTable[i].mpirank);
  }

  for(intervalIter = mpiRankIntervals.begin(); intervalIter != mpiRankIntervals.end(); intervalIter++) {
    interval = *intervalIter;
    start = interval.first;
    end = interval.second;

    if(start == end) {
      // Single rank
      if(allMPIRanks.find(start) != allMPIRanks.end()) {
        subsetMPIRanks.insert(start);
        allMPIRanks.erase(start);
      }

    } else {
      set<mpi_rank_t>::iterator rankIter;

      // Rank range
      int rank;
      for(rankIter = allMPIRanks.begin(); rankIter != allMPIRanks.end(); rankIter++) {
        rank = *rankIter;

        if((rank >= start) && (rank <= end)) {
          
          subsetMPIRanks.insert(rank);
        }

        // No more ranks to add for this range
        if(rank > end) {
          break;
        }
      }
    }
  }

  return true;
}

bool Group::anyTargetsAttached() {
  return (!subsetMPIRanks.empty());
}

bool Group::getAttached(Dyninst::ProcControlAPI::ProcessSet::ptr& lprocset) {
  assert(mpiRankToProcessMap != 0);

  if(!procSubset) {
    set<Process::ptr> processes;

    set<mpi_rank_t>::iterator subsetIter;
    map<int, Process::ptr>::iterator processIter;
    int rank;

    Err::info(true, "MPI subset size: %d", subsetMPIRanks.size());

    for(subsetIter = subsetMPIRanks.begin(); subsetIter != subsetMPIRanks.end(); subsetIter++) {
      rank = *subsetIter;

      processIter = mpiRankToProcessMap->find(rank);
      if(processIter == mpiRankToProcessMap->end()) {
        Err::warn(false, "Process structure for MPI Rank %d could not be found", rank);
      } else {
        Process::ptr process = processIter->second;
        processes.insert(process);
      }
    }

    if(processes.empty()) {
      Err::warn(false, "No processes found for rank subset");
    } else {
      procSubset = ProcessSet::newProcessSet(processes);
      lprocset = procSubset;

      return true;
    }
  } else {
    lprocset = procSubset;
    return true;
  }
}

DysectErrorCode Group::createStream()   { return OK; }

DysectErrorCode Group::prepareStream() {
  if(!resolveExpr()) {
    return Err::warn(DomainExpressionError, "Failed resolving group expression: \"%s\"", groupExpr.c_str());
  }

  lookupAttached();

  // All ids are already created
  return OK;
}
