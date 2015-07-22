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

#include "DysectAPI/Aggregates/Aggregate.h"
#include "DysectAPI/Aggregates/RankListAgg.h"
#include "DysectAPI.h"

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

bool RankListAgg::collect(void* process, void *thread) {
  Process::const_ptr process_ptr = *(Process::const_ptr*)process;
  Thread::const_ptr thread_ptr = *(Thread::const_ptr*)thread;

  if(!process_ptr) {
    return DYSECTVERBOSE(false, "Process object not available");
  }

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

  DYSECTVERBOSE(true, "found rank %d", rank);
  rankList.push_back(rank);

  return true;
}

