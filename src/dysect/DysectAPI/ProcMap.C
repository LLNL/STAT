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

#include "DysectAPI/ProcMap.h"

using namespace std;

ProcMap ProcMap::instance;

void ProcMap::addProcess(Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::Stackwalker::Walker* walker, BPatch_process* dyninst_proc, int rank) {
  walkers[process] = walker;
  dyninst_procs[process] = dyninst_proc;
  ranks[process] = rank;
}

Dyninst::Stackwalker::Walker* ProcMap::getWalker(Dyninst::ProcControlAPI::Process::const_ptr process) {
  return walkers[process];
}

BPatch_process* ProcMap::getDyninstProcess(Dyninst::ProcControlAPI::Process::const_ptr process) {
  return dyninst_procs[process];
}

int ProcMap::getRank(Dyninst::ProcControlAPI::Process::const_ptr process) {
  return ranks[process];
}

ProcMap* ProcMap::get() {
  return &ProcMap::instance;
}

Dyninst::ProcControlAPI::Process::const_ptr ProcMap::getProcControlProcess(BPatch_process* dyninst_proc) {
  map<Dyninst::ProcControlAPI::Process::const_ptr, BPatch_process*>::iterator iter;
  for (iter = dyninst_procs.begin(); iter != dyninst_procs.end(); ++iter) {
    if (iter->second == dyninst_proc) {
      return iter->first;
    }
  }

  throw exception();
}
