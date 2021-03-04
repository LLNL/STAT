#include "STAT_ctiFrontEnd.h"

#define ctiError()   (printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "CTI Error: %s\n", cti_error_str()),STAT_SYSTEM_ERROR)

static void unimplemented(const char* str) { fprintf(stderr, "unimplemented: %s\n", str); }

STAT_ctiFrontEnd::STAT_ctiFrontEnd() : appId_(0), session_(0), numPEs_(0), hosts_(nullptr),
                                       tasksPerPE_(1)
{
}

STAT_ctiFrontEnd::~STAT_ctiFrontEnd()
{
    if (hosts_) cti_destroyHostsList(hosts_);
}

StatError_t STAT_ctiFrontEnd::setupForSerialAttach()
{
    printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Serial launch is not supported in CTI\n");
    return STAT_ARG_ERROR;
}

StatError_t STAT_ctiFrontEnd::attach()
{
    // register the app with CTI
    void* vops = nullptr;
    auto wlm = cti_open_ops(&vops);

    if (!vops) {
        return ctiError();
    }

    switch (wlm) {
    case CTI_WLM_SLURM:
    {
        auto ops = static_cast<cti_slurm_ops_t *>(vops);
        cti_srunProc_t *srunInfo = ops->getJobInfo(launcherPid_);
        if (!srunInfo)
            return ctiError();

        appId_ = ops->registerJobStep(srunInfo->jobid, srunInfo->stepid);
        free(srunInfo);

        if (!appId_)
            return ctiError();

        break;
    }
    case CTI_WLM_SSH:
    {
        auto ops = static_cast<cti_ssh_ops_t *>(vops);
        appId_ = ops->registerJob((pid_t)launcherPid_);
        if (!appId_) {
            return ctiError();
        }
        break;
    }
    case CTI_WLM_ALPS:
    {
        auto ops = static_cast<cti_alps_ops_t *>(vops);
        uint64_t apid = ops->getApid((pid_t)launcherPid_);
        if (apid == 0) {
            return ctiError();
        }
        appId_ = ops->registerApid(apid);
        if (appId_ == 0)
        {
            return ctiError();
        }
        break;
    }

    case CTI_WLM_PALS:
    {
        auto ops = static_cast<cti_pals_ops_t*>(vops);

        char* apid = ops->getApid((pid_t)launcherPid_);
        if (!apid) {
            return ctiError();
        }

        appId_ = ops->registerApid(apid);

        free(apid);
        if (!appId_) {
            return ctiError();
        }
    }

    default:
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "Unsupported Cray WLM!\n");
        return STAT_SYSTEM_ERROR;
    }
    return STAT_OK;
}

StatError_t STAT_ctiFrontEnd::launchDaemons()
{
    if (!toolDaemonExe_) {
        printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Tool daemon path not set\n");
        return STAT_ARG_ERROR;
    }

    if (applicationOption_ == STAT_SERIAL_ATTACH || applicationOption_ == STAT_SERIAL_GDB_ATTACH) {
        printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Serial launch is not supported in CTI\n");
        return STAT_ARG_ERROR;
    }

    // initialize cti app id
    StatError_t statError = attach();
    if (statError != STAT_OK) {
        return statError;
    }

    /* Increase the max proc and max fd limits for MRNet threads */
#if (defined(HAVE_GETRLIMIT) && defined(HAVE_SETRLIMIT))
    statError = increaseSysLimits();
    if (statError != STAT_OK)
        printMsg(statError, __FILE__, __LINE__, "Failed to increase limits... attempting to run with current configuration\n");
#endif

    const char* defaultArgv[] = { nullptr }, **argv = defaultArgv;;

    const char* pythonPath = getenv("PYTHONPATH");
    if (!pythonPath)
        pythonPath = ":";

    const char* gdbCommand = getenv("STAT_GDB");
    if (!gdbCommand)
        gdbCommand = "gdb";

    const char* gdbArgv[] = { "-P", pythonPath, "-G", gdbCommand, nullptr };

    if (applicationOption_ == STAT_GDB_ATTACH) {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Using STAT GDB attach %s and PYTHONPATH %s\n", gdbCommand, pythonPath);
        argv = gdbArgv;
    }

    session_ = cti_createSession(appId_);
    if (!session_)
        return ctiError();

    cti_manifest_id_t manifest = cti_createManifest(session_);
    if (!manifest)
        return ctiError();

    if (cti_addManifestBinary(manifest, toolDaemonExe_))
        return ctiError();

    if (cti_addManifestLibrary(manifest, filterPath_))
        return ctiError();

    if (cti_sendManifest(manifest))
        return ctiError();

    char* cpExe = strdup(toolDaemonExe_);
    char* toolExe = basename(cpExe);

    if (cti_execToolDaemon(manifest, toolExe, argv, nullptr)) {
        free(cpExe);
        return ctiError();
    }
    
    free(cpExe);

    statError = getProcInfo();
    if (statError != STAT_OK) {
        return statError;
    }
    
    return STAT_OK;
}

StatError_t STAT_ctiFrontEnd::getProcInfo()
{
    numPEs_ = cti_getNumAppPEs(appId_);
    if (!numPEs_)
        return ctiError();

    hosts_ = cti_getAppHostsPlacement(appId_);
    if (!hosts_)
        return ctiError();

    int numHosts = hosts_->numHosts;
    procToHost_.reserve(numHosts);

    int totNumPEs = 0;
    for (int i=0, totNumPEs=0; i<numHosts; ++i) {
        totNumPEs += hosts_->hosts[i].numPes;
        procToHost_.push_back(totNumPEs);
    }
    return STAT_OK;
}

StatError_t STAT_ctiFrontEnd::sendDaemonInfo()
{
    unimplemented("STAT_ctiFrontEnd::sendDaemonInfo");
    return STAT_OK;
}

StatError_t STAT_ctiFrontEnd::createMRNetNetwork(const char* topologyFileName)
{
    // the filter should have already been sent in the manifest
    network_ = MRN::Network::CreateNetworkFE(topologyFileName, NULL, NULL);
    return STAT_OK;
}


void STAT_ctiFrontEnd::detachFromLauncher(const char* errMsg)
{
    if (session_) {
        if (cti_destroySession(session_))
            printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "Detach failed %s\n", errMsg);
    }
}



void STAT_ctiFrontEnd::shutDown() { unimplemented("shutDown"); }

bool STAT_ctiFrontEnd::daemonsHaveExited() {
    return !cti_appIsValid(appId_);
}
bool STAT_ctiFrontEnd::isKilled() {
    return !cti_appIsValid(appId_);
}

int STAT_ctiFrontEnd::getNumProcs() {
    return numPEs_;
}
const char* STAT_ctiFrontEnd::getHostnameForProc(int procIdx) {
    if (!hosts_) {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "proc table is not initialized\n");
        return "invalid process";
    }

    // for STATBench, 
    procIdx /= tasksPerPE_;     

    if (procIdx < 0 || procIdx >= numPEs_) {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "invalid procIdx in getHostnameForProc\n");
        return "invalid process";
    }

    auto hostIdx = std::upper_bound(procToHost_.begin(), procToHost_.end(), procIdx) -
        procToHost_.begin();

    return hosts_->hosts[hostIdx].hostname;
};

int STAT_ctiFrontEnd::getMpiRankForProc(int procIdx)
{
    static int cnt = 0;
    if (!cnt++)
        unimplemented("getMpiRankForProc");
    return procIdx;
}

StatError_t STAT_ctiFrontEnd::dumpProctab()
{
    char fileName[BUFSIZE];
    snprintf(fileName, BUFSIZE, "%s/%s.ptab", outDir_, filePrefix_);

    FILE* file = fopen(fileName, "w");
    if (!file) {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: fopen failed to create ptab file %s\n", strerror(errno), fileName);
        return STAT_FILE_ERROR;
    }

    if (!hosts_) {
        fprintf(file, "host names are unavailable\n");
    } else {
        int totPEs = 0;
        for (int i=0, n=hosts_->numHosts; i<n; ++i) {
            int pes = hosts_->hosts[i].numPes;
            fprintf(file, "%d - %d : %s" , totPEs, totPEs+pes-1, hosts_->hosts[i].hostname);
            totPEs += pes;
        }
    }

    fclose(file);
    return STAT_OK;
}

StatError_t STAT_ctiFrontEnd::addSerialProcess(const char *pidString)
{
    printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Serial launch is not supported in CTI\n");
    return STAT_ARG_ERROR;
}
StatError_t STAT_ctiFrontEnd::addDaemonSerialProcArgs(int& deamonArgc, char ** &deamonArgv)
{
    printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Serial launch is not supported in CTI\n");
    return STAT_ARG_ERROR;
}

StatError_t STAT_ctiFrontEnd::setAppNodeList()
{
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Generating application node list\n");
    applicationNodeMultiSet_.clear();

    if (!hosts_) {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "host table is not available\n");
        return STAT_SYSTEM_ERROR;
    }

    int numHosts = hosts_->numHosts;
    for (int i=0; i<numHosts; ++i) {
        applicationNodeMultiSet_.insert(hosts_->hosts[i].hostname);
    }

    nApplNodes_ = numHosts;

    return STAT_OK;
}

StatError_t STAT_ctiFrontEnd::STATBench_setAppNodeList()
{
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Generating application node list\n");
    applicationNodeMultiSet_.clear();

    if (!hosts_) {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "host table is not available\n");
        return STAT_SYSTEM_ERROR;
    }

    for (int i=0, n=hosts_->numHosts; i<n; ++i) {
        const std::string host = hosts_->hosts[i].hostname;
        for (int j=0, numPEs=hosts_->hosts[i].numPes; j<numPEs; ++j) {
            applicationNodeMultiSet_.insert(host);
        }
    }

    nApplNodes_ = nApplProcs_;

    return STAT_OK;
}
StatError_t STAT_ctiFrontEnd::STATBench_resetProctab(unsigned int nTasks) {
    tasksPerPE_ = nTasks;
    nApplProcs_ = nApplNodes_ * nTasks;
    return STAT_OK;
}

STAT_FrontEnd* STAT_FrontEnd::make()
{
    return new STAT_ctiFrontEnd();
}
