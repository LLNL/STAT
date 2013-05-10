#include "DysectAPI/DysectAPIBE.h"
#include "STAT_BackEnd.h"
#include "libDysectAPI/include/DysectAPI/Backend.h"
#include <unistd.h>

using namespace std;
using namespace DysectAPI;
using namespace Dyninst;
using namespace ProcControlAPI;

//Dyninst::ProcControlAPI::ProcessSet::ptr BE::procset;
STAT_BackEnd *BE::statBE;

BE::BE(const char* libPath, STAT_BackEnd* be) : loaded(false) {
  assert(be != 0);
  assert(libPath != 0);

  sessionLib = new SessionLibrary(libPath);

  if(!sessionLib->isLoaded()) {
    loaded = false;
    return ;
  }

  DysectAPI::defaultProbes();

  if(sessionLib->mapMethod<proc_start_t>("on_proc_start", &lib_proc_start) != OK) {
    loaded = false;
    return ;
  }

  struct DysectBEContext_t beContext;
  beContext.processTable = be->proctab_;
  beContext.processTableSize = be->proctabSize_;
  beContext.walkerSet = be->walkerSet_;
  beContext.hostname = (char*)&(be->localHostName_);
  beContext.mpiRankToProcessMap = &(be->mpiRankToProcessMap);

  statBE = be;

  bool useMrnetPrintf = false;
  if (be->logType_ & STAT_LOG_MRN)
    useMrnetPrintf = true;
  Err::init(be->errOutFp_, gStatOutFp, useMrnetPrintf);

  // Setup session
  lib_proc_start();

  if(Backend::registerEventHandlers() != DysectAPI::OK) {
    loaded = false;
    return ;
  }

  if(Backend::prepareProbes(&beContext) != DysectAPI::OK) {
    loaded = false;
    return ;
  }

  pthread_mutex_init(&Probe::requestQueueMutex, NULL);

  // Avoid trouble with hanging processes
  // Detach properly from connected processes
  signal(SIGHUP,    BE::gracefulShutdown);
  signal(SIGINT,    BE::gracefulShutdown);
  signal(SIGQUIT,   BE::gracefulShutdown);
  //signal(SIGILL,    BE::gracefulShutdown);
  signal(SIGTRAP,   BE::gracefulShutdown);
  signal(SIGABRT,   BE::gracefulShutdown); // Assert signal
  signal(SIGFPE,    BE::gracefulShutdown);
  signal(SIGUSR1,   BE::gracefulShutdown);
  signal(SIGCONT,   BE::gracefulShutdown); // When backend is continued by TV
  signal(SIGSEGV,   BE::gracefulShutdown);
  signal(SIGBUS,    BE::gracefulShutdown);
  signal(SIGUSR2,   BE::gracefulShutdown);
  signal(SIGPIPE,   BE::gracefulShutdown);
  signal(SIGALRM,   BE::gracefulShutdown);
  signal(SIGSTKFLT, BE::gracefulShutdown);
  signal(SIGTSTP,   BE::gracefulShutdown);
  signal(SIGURG,    BE::gracefulShutdown);
  signal(SIGXCPU,   BE::gracefulShutdown);
  signal(SIGXFSZ,   BE::gracefulShutdown);
  signal(SIGVTALRM, BE::gracefulShutdown);


  loaded = true;
}

BE::~BE() {
  if(sessionLib) {
    // Close active dynamic library
    delete(sessionLib);
  }
}

bool BE::isLoaded() {
  return loaded;
}

DysectErrorCode BE::relayPacket(MRN::PacketPtr* packet, int tag, MRN::Stream* stream) {
  
  return Backend::relayPacket(packet, tag, stream); 

}

DysectErrorCode BE::handleTimerEvents() {
  return Backend::handleTimerEvents();
}

DysectErrorCode BE::handleQueuedOperations() {
  return Backend::handleQueuedOperations();
}

void BE::gracefulShutdown(int signal) {
  static bool called = false;

  if(!called) {
    called = true;

    Err::verbose(false, "Backend caugth signal %d - shutting down", signal);
  
    ProcessMgr::detachAll(); 

    pthread_mutex_destroy(&Probe::requestQueueMutex);
  }

  if(signal != 18) {
    Err::info("Backend shutdown due to signal %d", signal);
  }
  sleep(10);

  Err::info("Throwing SIGILL to get stacktrace");
  raise(SIGILL);

  exit(EXIT_SUCCESS);
}


void BE::enableTimers() {
  //Timer::unblockTimers();
}

void BE::disableTimers() {
  //Timer::blockTimers();
}
