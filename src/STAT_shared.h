#ifndef __STAT_SHARED_H
#define __STAT_SHARED_H

#include "mrnet/MRNet.h"
#include "lmon_api/lmon_fe.h"

#include "PlatFeatures.h"
#include "ProcessSet.h"
#include "PCErrors.h"

#include "Symtab.h"
#include "walker.h"
#include "procstate.h"
#include "frame.h"
#include "swk_errors.h"
#include "Type.h"

namespace DysectAPI {
  typedef enum RunTimeErrorCode {
    OK,
    Error,
    InvalidSystemState,
    LibraryNotLoaded,
    SymbolNotFound,
    SessionCont,
    SessionQuit,
    DomainNotFound,
    NetworkError,
    DomainExpressionError,
    StreamError,
    OptimizedOut,
  } DysectErrorCode;
}

typedef struct
{
    int count;
    int *list;
} IntList_t;

struct DysectFEContext_t {
  MRN::Network* network;
  MPIR_PROCDESC_EXT *processTable;
  int processTableSize;
  std::map<int, IntList_t *>* mrnetRankToMpiRanksMap;
  int upstreamFilterId;
  class STAT_FrontEnd* statFE;
};

struct DysectBEContext_t {
  MPIR_PROCDESC_EXT *processTable;
  int processTableSize;
  Dyninst::Stackwalker::WalkerSet *walkerSet;
  char *hostname;
  std::map<int, Dyninst::ProcControlAPI::Process::ptr> *mpiRankToProcessMap;
  class STAT_BackEnd* statBE;
};

const int DysectGlobalReadyTag     = 0x7e000009;
const int DysectGlobalReadyRespTag = 0x7e00000A;
const int DysectGlobalStartTag =     0x7e00000B;

#endif
