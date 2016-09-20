
#ifndef __PROCMAP_H
#define __PROCMAP_H

#include <map>

#include "walker.h"
#include "BPatch.h"

class ProcMap {
  static ProcMap instance;

  std::map<Dyninst::ProcControlAPI::Process::const_ptr, Dyninst::Stackwalker::Walker*> walkers;
  std::map<Dyninst::ProcControlAPI::Process::const_ptr, BPatch_process*> dyninst_procs;
  std::map<Dyninst::ProcControlAPI::Process::const_ptr, int> ranks;

public:
  void addProcess(Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::Stackwalker::Walker* walker, BPatch_process* dyninst_proc, int rank);

  Dyninst::Stackwalker::Walker* getWalker(Dyninst::ProcControlAPI::Process::const_ptr process);
  BPatch_process* getDyninstProcess(Dyninst::ProcControlAPI::Process::const_ptr process);
  Dyninst::ProcControlAPI::Process::const_ptr getProcControlProcess(BPatch_process* dyninst_proc);
  int getRank(Dyninst::ProcControlAPI::Process::const_ptr process);

  static ProcMap* get();
};

#endif // __PROCMAP_H
