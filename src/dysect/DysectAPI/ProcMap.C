
#include "DysectAPI/ProcMap.h"

ProcMap ProcMap::instance;

void ProcMap::addProcess(Dyninst::ProcControlAPI::Process::const_ptr process, Dyninst::Stackwalker::Walker* walker, BPatch_process* dyninst_proc) {
  walkers[process] = walker;
  dyninst_procs[process] = dyninst_proc;
}

Dyninst::Stackwalker::Walker* ProcMap::getWalker(Dyninst::ProcControlAPI::Process::const_ptr process) {
  return walkers[process];
}

BPatch_process* ProcMap::getDyninstProcess(Dyninst::ProcControlAPI::Process::const_ptr process) {
  return dyninst_procs[process];
}

ProcMap* ProcMap::get() {
  return &ProcMap::instance;
}
