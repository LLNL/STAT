/*
Copyright (c) 2007-2008, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory
Written by Gregory Lee <lee218@llnl.gov>, Dorian Arnold, Dong Ahn, Bronis de Supinski, Barton Miller, and Martin Schulz.
LLNL-CODE-400455.
All rights reserved.

This file is part of STAT. For details, see http://www.paradyn.org/STAT. Please also read STAT/LICENSE.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice, this list of conditions and the disclaimer below.
        Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the disclaimer (as noted below) in the documentation and/or other materials provided with the distribution.
        Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
        
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "STAT_FrontEnd.h"

using namespace std;
using namespace MRN;

#ifdef STAT_FGFS
    using namespace FastGlobalFileStat;
    using namespace FastGlobalFileStat::MountPointAttribute;
    using namespace FastGlobalFileStat::CommLayer;
    using namespace FastGlobalFileStat::MountPointAttribute;
#endif

static int lmonState = 0;
STAT_timer totalStart, totalEnd, startTime, endTime;

STAT_FrontEnd::STAT_FrontEnd()
{
    int ret;
    char tmp[BUFSIZE], *envValue;

    /* Enable MRNet logging if requested */
    envValue = getenv("STAT_MRNET_OUTPUT_LEVEL");
    if (envValue != NULL)
    {
        mrnetOutputLevel_ = atoi(envValue);
        set_OutputLevel(mrnetOutputLevel_);
    }
    envValue = getenv("STAT_MRNET_DEBUG_LOG_DIRECTORY");
    if (envValue != NULL)
        setenv("MRNET_DEBUG_LOG_DIRECTORY", envValue, 1);
    
    /* Set MRNet and LMON rsh command */
    envValue = getenv("STAT_LMON_REMOTE_LOGIN");
    if (envValue != NULL)
        setenv("LMON_REMOTE_LOGIN", envValue, 1);
    envValue = getenv("STAT_XPLAT_RSH");
    if (envValue != NULL)
        setenv("XPLAT_RSH", envValue, 1);

    /* Set LMON_DEBUG_BES */
    envValue = getenv("STAT_LMON_DEBUG_BES");
    if (envValue != NULL)
        setenv("LMON_DEBUG_BES", envValue, 1);

    /* Set the launchmon and mrnet_commnode paths based on STAT env vars */
    envValue = getenv("STAT_LMON_LAUNCHMON_ENGINE_PATH");
    if (envValue != NULL)
        setenv("LMON_LAUNCHMON_ENGINE_PATH", envValue, 1);
    envValue = getenv("STAT_MRN_COMM_PATH");
    if (envValue != NULL)
    {
        setenv("MRN_COMM_PATH", envValue, 1); // for MRNet < 3.0.1
        setenv("MRNET_COMM_PATH", envValue, 1);
    }
    envValue = getenv("STAT_MRNET_COMM_PATH");
    if (envValue != NULL)
    {
        setenv("MRN_COMM_PATH", envValue, 1); // for MRNet > 3.0.1
        setenv("MRNET_COMM_PATH", envValue, 1);
    }

    /* Set MRNet configuration parameters */
    envValue = getenv("STAT_MRNET_STARTUP_TIMEOUT");
    if (envValue != NULL)
        setenv("MRNET_STARTUP_TIMEOUT", envValue, 1);
    envValue = getenv("STAT_MRNET_PORT_BASE");
    if (envValue != NULL)
        setenv("MRNET_PORT_BASE", envValue, 1);

    /* Set the daemon path and filter paths to the environment variable 
       specification if applicable.  Otherwise, set to default install 
       directory */
    envValue = getenv("STAT_DAEMON_PATH");
    if (envValue != NULL)
    {
        toolDaemonExe_ = strdup(envValue);
        if (toolDaemonExe_ == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed on call to set tool daemon exe with strdup()\n", strerror(errno));
            exit(-1);
        }
    }
    else
    {
        if (strlen(getInstallPrefix()) > 1)
            snprintf(tmp, BUFSIZE, "%s/bin/STATD", getInstallPrefix());
        else
            snprintf(tmp, BUFSIZE, "STATD");
        toolDaemonExe_ = strdup(tmp);
        if (toolDaemonExe_ == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed on call to set tool daemon exe with strdup()\n", strerror(errno));
            exit(-1);
        }
    }
 
    envValue = getenv("STAT_FILTER_PATH");
    if (envValue != NULL)
    {
        filterPath_ = strdup(envValue);
        if (filterPath_ == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed on call to set tool filter path with strdup()\n", strerror(errno));
            exit(-1);
        }
    }
    else
    {
        if (strlen(getInstallPrefix()) > 1)
            snprintf(tmp, BUFSIZE, "%s/lib/STAT_FilterDefinitions.so", getInstallPrefix());
        else
            snprintf(tmp, BUFSIZE, "STAT_FilterDefinitions.so");
        filterPath_ = strdup(tmp);
        if (filterPath_ == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed on call to set filter path with strdup()\n", strerror(errno));
            exit(-1);
        }
    }

#ifdef STAT_FGFS
    envValue = getenv("STAT_FGFS_FILTER_PATH");
    if (envValue != NULL)
    {
        fgfsFilterPath_ = strdup(envValue);
        if (fgfsFilterPath_ == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed on call to set FGFS filter path with strdup()\n", strerror(errno));
            exit(-1);
        }
    }
    else
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: STAT_FGFS_FILTER_PATH environment variable not set\n");
        exit(-1);
    }
#endif

    /* Get the FE hostname */
#ifdef CRAYXT
    string temp;
    ret = XPlat::NetUtils::GetLocalHostName(temp);
    snprintf(hostname_, BUFSIZE, "%s", temp.c_str());
#else
    ret = gethostname(hostname_, BUFSIZE);
#endif
    if (ret != 0)
        fprintf(stderr, "Warning, Failed to get hostname\n");

    /* Initialize variables */
    verbose_ = STAT_VERBOSE_STDOUT;
    isLaunched_ = false;
    isAttached_ = false;
    isConnected_ = false;
    isPendingAck_ = false;
    hasFatalError_ = false;
    mergeStream_ = NULL;
    broadcastStream_ = NULL;
    broadcastCommunicator_ = NULL;
    network_ = NULL;
    statOutFp = NULL;
    launcherPid_ = 0;
    launcherArgv_ = NULL;
    launcherArgc_ = 1;
    logging_ = STAT_LOG_NONE;
    applExe_ = NULL;
    jobId_ = 0;
    proctabSize_ = 0;
    proctab_ = NULL;
    remoteNode_ = NULL;
    isRunning_ = false;
    snprintf(outDir_, BUFSIZE, "NULL");
    snprintf(logOutDir_, BUFSIZE, "NULL");
    snprintf(filePrefix_, BUFSIZE, "NULL");
#ifdef STAT_PROCS_PER_NODE
    procsPerNode_ = STAT_PROCS_PER_NODE;
#else
    procsPerNode_ = 1;
#endif    
    envValue = getenv("STAT_PROCS_PER_NODE");
    if (envValue != NULL)
        procsPerNode_ = atoi(envValue);
}

STAT_FrontEnd::~STAT_FrontEnd()
{
    unsigned int i;
    StatError_t statError;
    map<string, IntList_t *>::iterator iter;

    /* Dump the performance metrics to a file */
    statError = dumpPerf();
    if (statError != STAT_OK)
        printMsg(statError, __FILE__, __LINE__, "Failed to dump performance results\n");

    /* Free up MRNet variables */
    if (network_ != NULL)
        delete network_;
    
    /* Free up any allocated variables */
    if (toolDaemonExe_ != NULL)
        free(toolDaemonExe_);
    if (filterPath_ != NULL)
        free(filterPath_);
#ifdef STAT_FGFS        
    if (fgfsFilterPath_ != NULL)
        free(fgfsFilterPath_);
#endif
    if (applExe_ != NULL)
        free(applExe_);
    if (proctab_ != NULL)
    {
        for (i = 0; i < proctabSize_; i++)
        {
            if (proctab_[i].pd.executable_name != NULL)
                free(proctab_[i].pd.executable_name);
            if (proctab_[i].pd.host_name != NULL)
                free(proctab_[i].pd.host_name);
        }
        free(proctab_);
    }
    if (launcherArgv_ != NULL)
    {
        for (i = 0; i < launcherArgc_; i++)
        {
            if (launcherArgv_[i] != NULL)
                free(launcherArgv_[i]);
        }
        free(launcherArgv_);
    }    
    isAttached_ = false;
    isConnected_ = false;
    graphlib_Finish();
}

StatError_t STAT_FrontEnd::attachAndSpawnDaemons(unsigned int pid, char *remoteNode)
{
    StatError_t statError;

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Attaching to job launcher and launching tool daemons...\n");

    launcherPid_ = pid;
    if (remoteNode_ != NULL)
        free(remoteNode_);
    if (remoteNode != NULL)
        remoteNode_ = strdup(remoteNode);
    else
        remoteNode_ = NULL;

    statError = launchDaemons(STAT_ATTACH);
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to attach and spawn daemons\n");
        return statError;
    }

    return STAT_OK;
}

StatError_t STAT_FrontEnd::launchAndSpawnDaemons(char *remoteNode, bool isStatBench)
{
    StatError_t statError;

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Launching applicaiton and tool daemons...\n");

    if (remoteNode_ != NULL)
        free(remoteNode_);
    if (remoteNode != NULL)
        remoteNode_ = strdup(remoteNode);
    else
        remoteNode_ = NULL;

    statError = launchDaemons(STAT_LAUNCH, isStatBench);
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to attach and spawn daemons\n");
        return statError;
    }

    return STAT_OK;
}

StatError_t STAT_FrontEnd::launchDaemons(StatLaunch_t applicationOption, bool isStatBench)
{
    int i;
    unsigned int proctabSize;
    char **daemonArgv = NULL;
    static bool firstRun = true;
    lmon_rc_e rc;
    lmon_rm_info_t rmInfo;
    StatError_t statError;

    /* Initialize performance timer */
    addPerfData("MRNet Launch Time", -1.0);
    totalStart.setTime();

    /* Increase the max proc and max fd limits */
#if (defined(HAVE_GETRLIMIT) && defined(HAVE_SETRLIMIT))
    statError = increaseSysLimits();
    if (statError != STAT_OK)
        printMsg(statError, __FILE__, __LINE__, "Failed to increase limits... attemting to run with current configuration\n");
#endif    

    startTime.setTime();

    /* Initialize LaunchMON */
    if (firstRun == true)
    {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Initializing LaunchMON\n");
        rc = LMON_fe_init(LMON_VERSION);
        if (rc != LMON_OK)
        {
            printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to initialize Launchmon\n");
            return STAT_LMON_ERROR;
        }
    }
    firstRun = false;

    /* Create a LaunchMON session */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Creating LaunchMON session\n");
    rc = LMON_fe_createSession(&lmonSession_);
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to create Launchmon session\n");
        return STAT_LMON_ERROR;
    }

    /* Register STAT's pack function */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Registering pack function with LaunchMON\n");
    rc = LMON_fe_regPackForFeToBe(lmonSession_, statPack);
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to register pack function\n");
        return STAT_LMON_ERROR;
    }

    /* Register STAT's check status function */
    lmonState = 0;
    if (isStatBench == false)
    {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Registering status CB function with LaunchMON\n");
        rc = LMON_fe_regStatusCB(lmonSession_, lmonStatusCb);
        if (rc != LMON_OK)
        {
            printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to register status CB function\n");
            return STAT_LMON_ERROR;
        }
    }
    else
        lmonState = lmonState | 0x00000002;

    /* Make sure we know which daemon to launch */
    if (toolDaemonExe_ == NULL)
    {
        printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Tool daemon path not set\n");
        return STAT_ARG_ERROR;
    }

    /* If we're logging the daemons, pass the output directory as a command line arg */
    if (logging_ & STAT_LOG_BE)
    {
        daemonArgv = (char **)malloc(6 * sizeof(char *));
        if (daemonArgv == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s malloc failed to allocate for daemon argv\n", strerror(errno));
            return STAT_ALLOCATE_ERROR;
        }
        daemonArgv[0] = "-L";
        daemonArgv[1] = logOutDir_;
        daemonArgv[2] = "-o";
        daemonArgv[3] = (char *)malloc(8 * sizeof(char));
        snprintf(daemonArgv[3], 8, "%d", mrnetOutputLevel_);
        if (logging_ & STAT_LOG_MRN)
        {
            daemonArgv[4] = "-m";
            daemonArgv[5] = NULL;
        }
        else
            daemonArgv[4] = NULL;
    }        

    if (applicationOption == STAT_ATTACH)
    {
        /* Make sure the pid has been set */
        if (launcherPid_ == 0)
        {
            printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Launcher PID not set\n");
            return STAT_ARG_ERROR;
        }

        /* Attach to launcher and spawn daemons */
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Attaching to job launcher and spawning daemons\n");
        rc = LMON_fe_attachAndSpawnDaemons(lmonSession_, remoteNode_, launcherPid_, toolDaemonExe_, daemonArgv, NULL, NULL);
        if (rc != LMON_OK)
        {
            printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to attach to job launcher and spawn daemons\n");
            return STAT_LMON_ERROR;
        }
    }
    else
    {
        /* Launch the application and spawn daemons */
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Launching the application and spawning daemons\n");
        i = 0;
        while(1)
        {
            if (launcherArgv_[i] == NULL)
                break;
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Launcher arg %d = %s\n", i, launcherArgv_[i]);
            i++;
        }
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "remote node = %s, daemon = %s\n", remoteNode_, toolDaemonExe_);
        rc = LMON_fe_launchAndSpawnDaemons(lmonSession_, remoteNode_, launcherArgv_[0], launcherArgv_, toolDaemonExe_, daemonArgv, NULL, NULL);
        if (rc != LMON_OK)
        {
            printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to launch job and spawn daemons\n");
            return STAT_LMON_ERROR;
        }

        /* Get the launcher PID */
        rc = LMON_fe_getRMInfo(lmonSession_, &rmInfo);
        if (rc != LMON_OK)
        {
            printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to get resource manager info\n");
            return STAT_LMON_ERROR;
        }
        launcherPid_ = rmInfo.rm_launcher_pid;
    }

    /* Gather the process table */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Gathering the process table\n");
    rc = LMON_fe_getProctableSize(lmonSession_, &proctabSize_);
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to get Process Table Size\n");
        return STAT_LMON_ERROR;
    }
    proctab_ = (MPIR_PROCDESC_EXT *)malloc(proctabSize_ * sizeof(MPIR_PROCDESC_EXT));
    if (proctab_ == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to allocate memory for the process table\n", strerror(errno));
        return STAT_ALLOCATE_ERROR;
    }
    rc = LMON_fe_getProctable(lmonSession_, proctab_, &proctabSize, proctabSize_);
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to get Process Table\n");
        return STAT_LMON_ERROR;
    }

    /* Gather application characteristics */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Gathering application information\n");

    /* TODO: this only works for SIMD applications */
    applExe_ = strdup(proctab_[0].pd.executable_name);
    if (applExe_ == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to set applExe_ with strdup()\n");
        return STAT_ALLOCATE_ERROR;
    }
    nApplProcs_ = proctabSize;
    isLaunched_ = true;
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Daemons successfully launched\n");

    endTime.setTime();
    addPerfData("\tLaunchmon Launch Time", (endTime - startTime).getDoubleTime());
    printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "\tTool daemons launched\n");

    /* Create the output directory and file prefix if not already created */
    if (strcmp(outDir_, "NULL") == 0 || strcmp(filePrefix_, "NULL") == 0)
    {
        statError = createOutputDir();
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Failed to create output directory\n");
            return statError;
        }
    }

    /* Dump the proctable to a file */
    statError = dumpProctab();
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to dump process table to a file\n");
        return statError;
    }

    /* Set the application node list based on the process table */
    printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "\tPopulating application node list\n");
    if (isStatBench == true)
    {
        startTime.setTime();
        statError = STATBench_setAppNodeList();
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Failed to set app node list\n");
            return statError;
        }
        endTime.setTime();
        addPerfData("\tApp Node List Creation Time", (endTime - startTime).getDoubleTime());
    }
    else
    {
        startTime.setTime();
        statError = setAppNodeList();
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Failed to set app node list\n");
            return statError;
        }
        endTime.setTime();
        addPerfData("\tApp Node List Creation Time", (endTime - startTime).getDoubleTime());
    }

    if (daemonArgv != NULL)
        free(daemonArgv);

    return STAT_OK;
}

#ifdef MRNET3
//! The number of BEs that have connected
int nCallbacks;
pthread_mutex_t mrnetCallbackMutex = PTHREAD_MUTEX_INITIALIZER;

void beConnectCb(Event *event, void *dummy)
{
    if ((event->get_Class() == Event::TOPOLOGY_EVENT) && (event->get_Type() == TopologyEvent::TOPOL_ADD_BE))
    {
        pthread_mutex_lock(&mrnetCallbackMutex);
        nCallbacks++;
        pthread_mutex_unlock(&mrnetCallbackMutex);
    }
}

void nodeRemovedCb(Event *event, void *dummy)
{
    StatError_t statError;
    if ((event->get_Class() == Event::TOPOLOGY_EVENT) && (event->get_Type() == TopologyEvent::TOPOL_REMOVE_NODE))
    {
        pthread_mutex_lock(&mrnetCallbackMutex);
        ((STAT_FrontEnd *)dummy)->printMsg(STAT_WARNING, __FILE__, __LINE__, "MRNet detected a tool process exit.  Recovering with available resources.\n");
        statError = ((STAT_FrontEnd *)dummy)->setRanksList();
        if (statError != STAT_OK)
        {
            ((STAT_FrontEnd *)dummy)->printMsg(statError, __FILE__, __LINE__, "An error occurred when trying to recover from node removal\n");
            ((STAT_FrontEnd *)dummy)->setHasFatalError(true);
        }
        pthread_mutex_unlock(&mrnetCallbackMutex);
    }
}

void topologyChangeCb(Event *event, void *dummy)
{
    StatError_t statError;
    if ((event->get_Class() == Event::TOPOLOGY_EVENT) && (event->get_Type() == TopologyEvent::TOPOL_CHANGE_PARENT))
    {
        pthread_mutex_lock(&mrnetCallbackMutex);
        ((STAT_FrontEnd *)dummy)->printMsg(STAT_WARNING, __FILE__, __LINE__, "MRNet detected a topology change.  Updating bit vector map.\n");
        statError = ((STAT_FrontEnd *)dummy)->setRanksList();
        if (statError != STAT_OK)
            ((STAT_FrontEnd *)dummy)->printMsg(statError, __FILE__, __LINE__, "An error occurred when trying to adjust to topology change\n");
        pthread_mutex_unlock(&mrnetCallbackMutex);
    }
}
#endif

StatError_t STAT_FrontEnd::launchMrnetTree(StatTopology_t topologyType, char *topologySpecification, char *nodeList, bool blocking, bool shareAppNodes, bool isStatBench)
{
    bool ret;
    char topologyFileName[BUFSIZE];
    StatError_t statError;

    /* Make sure daemons are launched */
    if (isLaunched_ == false)
    {
        printMsg(STAT_NOT_LAUNCHED_ERROR, __FILE__, __LINE__, "BEs not launched yet.\n");
        return STAT_NOT_LAUNCHED_ERROR;
    }
    
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "\tLaunching MRNet tree\n");
    startTime.setTime();

    /* Create a topology file */
    printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "Creating MRNet topology file\n");
    statError = createTopology(topologyFileName, topologyType, topologySpecification, nodeList, shareAppNodes);
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to create topology file\n");
        return statError;
    }
    endTime.setTime();
    addPerfData("\tCreate Topology File Time", (endTime - startTime).getDoubleTime());
    printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "\tMRNet topology %s created\n", topologyFileName);
   
    /* Now that we have the topology configured, launch away */
    printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "\tInitializing MRNet...\n");
    startTime.setTime();
#ifdef MRNET22
 #ifdef CRAYXT
  #ifdef MRNET31 
    map<string, string> attrs;
    char apidString[BUFSIZE];
    snprintf(apidString, BUFSIZE, "%d", launcherPid_);
    attrs.insert(make_pair("CRAY_ALPS_APRUN_PID", apidString));
    attrs.insert(make_pair("CRAY_ALPS_STAGE_FILES", filterPath_));
    network_ = Network::CreateNetworkFE(topologyFileName, NULL, NULL, &attrs);
  #else /* ifdef MRNET31 */
    map<string, string> attrs;
    char apidString[BUFSIZE], *emsg;
    int nid;
    uint64_t apid;
    emsg = alpsGetMyNid(&nid);
    if (emsg)
    {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "Failed to get nid\n");
        return STAT_SYSTEM_ERROR;
    }
    apid = alps_get_apid(nid, launcherPid_);
    if (apid <= 0)
    {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "Failed to get apid from aprun PID %d\n", launcherPid_);
        return STAT_SYSTEM_ERROR;
    }
    snprintf(apidString, BUFSIZE, "%d", apid);
    attrs["apid"] = apidString;
    network_ = Network::CreateNetworkFE(topologyFileName, NULL, NULL, &attrs);
  #endif /* ifdef MRNET31 */
 #else /* ifdef CRAYXT */
    network_ = Network::CreateNetworkFE(topologyFileName, NULL, NULL);
 #endif /* ifdef CRAYXT */
#else /* ifdef MRNET22 */
    network_ = new Network(topologyFileName, NULL, NULL);
#endif /* ifdef MRNET22 */ 
    if (network_ == NULL)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Network initialization failure\n");
        return STAT_MRNET_ERROR;
    }
    if (network_->has_Error() == true)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "MRNet reported a Network error with message: %s\n", network_->get_ErrorStr(network_->get_Error()));
        return STAT_MRNET_ERROR;
    }

#ifdef STAT_FGFS
    network_->set_FailureRecovery(false);
#endif

    endTime.setTime();
    addPerfData("\tMRNet Constructor Time", (endTime - startTime).getDoubleTime());
    printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "\tMRNet initialized\n");

#ifdef MRNET3
    /* Register the BE connect callback function with MRNet */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Registering topology event callback with MRNet\n");
    nCallbacks = 0;
    ret = network_->register_EventCallback(Event::TOPOLOGY_EVENT, TopologyEvent::TOPOL_ADD_BE, beConnectCb, NULL);
    if (ret == false) 
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to register MRNet BE connect event callback\n");
        return STAT_MRNET_ERROR;
    }

  #ifndef STAT_FGFS
    /* Register the node removed callback function with MRNet */
    ret = network_->register_EventCallback(Event::TOPOLOGY_EVENT, TopologyEvent::TOPOL_REMOVE_NODE, nodeRemovedCb, (void *)this);
    if (ret == false) 
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to register MRNet node removed callback\n");
        return STAT_MRNET_ERROR;
    }

    /* Register the topology change callback function with MRNet */
    ret = network_->register_EventCallback(Event::TOPOLOGY_EVENT, TopologyEvent::TOPOL_CHANGE_PARENT, topologyChangeCb, (void *)this);
    if (ret == false) 
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to register MRNet topology change callback\n");
        return STAT_MRNET_ERROR;
    }
  #endif    
#endif

    /* Get the topology information from the Network */
    leafInfo_.networkTopology = network_->get_NetworkTopology();
    if (leafInfo_.networkTopology == NULL)
    {
        network_->print_error("Failed to get MRNet Network Topology");
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Network topology gather failure\n");
        return STAT_MRNET_ERROR;
    }
    topologySize_ = leafInfo_.networkTopology->get_NumNodes();
    leafInfo_.daemons = applicationNodeMultiSet_;

    /* Send MRNet connection info to daemons */
    printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "\tConnecting to daemons...\n");
    startTime.setTime();
    if (isStatBench == false)
    {
        /* for STATbench we do this when creating traces since the proctab is modified */
        startTime.setTime();
        statError = createDaemonRankMap();
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Failed to create daemon rank map\n");
            return statError;
        }
        endTime.setTime();
        addPerfData("\tCreate Daemon Rank Map Time", (endTime - startTime).getDoubleTime());
    }

    leafInfo_.networkTopology->get_Leaves(leafInfo_.leafCps);

    statError = sendDaemonInfo();
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to Send Info to Daemons\n");
        return statError;
    }
    endTime.setTime();
    addPerfData("\tLaunchmon Send Time", (endTime - startTime).getDoubleTime());

    /* If we're blocking, wait for all BEs to connect to MRNet tree */
    if (blocking == true)
    {
        statError = connectMrnetTree(blocking, isStatBench);
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Failed to connect MRNet tree\n");
            return statError;
        }
    }

    return STAT_OK;
}

StatError_t STAT_FrontEnd::connectMrnetTree(bool blocking, bool isStatBench)
{
    static int connectTimeout = -1, i = 0;
    int filterId, retval, tag;
    char *connectTimeoutString, fullTopologyFile[BUFSIZE];
    StatError_t statError;
    PacketPtr packet;
#ifdef STAT_FGFS
    int filterId2;
    bool ret;
#endif

    /* Make sure the Network object has been created */
    if (network_ == NULL)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "MRNet Network object is NULL.\n");
        return STAT_MRNET_ERROR;
    }

    /* Connect to the Daemons */
    if (connectTimeout == -1)
    {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Connecting MRNet to daemons\n");
        startTime.setTime();
        connectTimeoutString = getenv("STAT_CONNECT_TIMEOUT");
        if (connectTimeoutString != NULL)
            connectTimeout = atoi(connectTimeoutString);
        else
            connectTimeout = 999999;
    }

    /* Check for BE connections */
    if (blocking == false)
    {
        i = i + 1;
        if (!WIFBESPAWNED(lmonState))
        {
            printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
            return STAT_DAEMON_ERROR;
        }
        if (i < connectTimeout * 100)
        {
#ifdef MRNET3        
            if (nCallbacks < nApplNodes_)
#else
            if (leafInfo_.networkTopology->get_NumNodes() < topologySize_ + nApplNodes_)
#endif
            {
                usleep(10000);
                return STAT_PENDING_ACK;
            }
        }
    }
    else
    {
        for (i = 0; i < connectTimeout * 100; i++)
        {
            if (!WIFBESPAWNED(lmonState))
            {
                printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
                return STAT_DAEMON_ERROR;
            }
#ifdef MRNET3        
            if (nCallbacks == nApplNodes_)
                break;
#else
            if (leafInfo_.networkTopology->get_NumNodes() >= topologySize_ + nApplNodes_)
                break;
#endif
            usleep(10000);
        }
    }

    /* Make sure all expected BEs registered within timeout limit */
#ifdef MRNET3        
    if (i >= connectTimeout * 100 || nCallbacks < nApplNodes_)
    {
            printMsg(STAT_WARNING, __FILE__, __LINE__, "Connection timed out after %d/%d seconds with %d of %d Backends reporting.\n", i/100, connectTimeout, nCallbacks, nApplNodes_);
        if (nCallbacks == 0)
            return STAT_DAEMON_ERROR;
        printMsg(STAT_WARNING, __FILE__, __LINE__, "Continuing with available subset.\n");
    }
#else
    if (i >= connectTimeout * 100 || leafInfo_.networkTopology->get_NumNodes() < topologySize_ + nApplNodes_)
    {
        printMsg(STAT_WARNING, __FILE__, __LINE__, "Connection timed out after %d/%d seconds with %d of %d Backends reporting.\n", i/100, connectTimeout, leafInfo_.networkTopology->get_NumNodes() - topologySize_, nApplNodes_);
        if (leafInfo_.networkTopology->get_NumNodes() <= topologySize_)
            return STAT_DAEMON_ERROR;
        printMsg(STAT_WARNING, __FILE__, __LINE__, "Continuing with available subset.\n");
    }
#endif

    endTime.setTime();
    addPerfData("\tConnect to Daemons Time", (endTime - startTime).getDoubleTime());
    printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "\tDaemons connected\n");

    /* Now that we're fully connected, determine the BE merge order */
    statError = setRanksList();
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to set ranks list\n");
        return statError;
    }

    /* Dump the fully connected topology to a file */
    snprintf(fullTopologyFile, BUFSIZE, "%s/%s.fulltop", outDir_, filePrefix_);
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Outputting full MRNet topology file to %s\n", fullTopologyFile);
    leafInfo_.networkTopology->print_TopologyFile(fullTopologyFile); 

    /* Get MRNet Broadcast Communicator */
    startTime.setTime();
    printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "\tConfiguring MRNet connection...\n");
    broadcastCommunicator_ = network_->get_BroadcastCommunicator();
    if (broadcastCommunicator_ == NULL)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to get MRNet Communicator\n");
        return STAT_MRNET_ERROR;
    }

    /* Check for the STAT filter path */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Loading STAT filter into MRNet\n");
    if (filterPath_ == NULL) 
    {
        printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Filter path not set\n");
        return STAT_ARG_ERROR;
    }

    /* Load the STAT filter into MRNet */
    filterId = network_->load_FilterFunc(filterPath_, "filterInit");
    if (filterId == -1)
    {
        printMsg(STAT_FILTERLOAD_ERROR, __FILE__, __LINE__, "load_FilterFunc() failure for path %s, function statMerge\n", filterPath_);
        return STAT_FILTERLOAD_ERROR;
    }
    
    /* Create broadcast stream */
    broadcastStream_ = network_->new_Stream(broadcastCommunicator_, TFILTER_SUM, SFILTER_WAITFORALL, filterId);
    if (broadcastStream_ == NULL)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to setup MRNet broadcast stream\n");
        return STAT_MRNET_ERROR;
    }

    /* Send an empty message using the broadcast stream */
    if (broadcastStream_->send(PROT_SEND_BROADCAST_STREAM, "%uc %s %d", logging_, logOutDir_, mrnetOutputLevel_) == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "failed to send on broadcast stream\n");
        return STAT_MRNET_ERROR;
    }
    if (broadcastStream_->flush() == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "failed to flush broadcast stream\n");
        return STAT_MRNET_ERROR;
    }

    //GLL TODO: should we get ack?
    retval = broadcastStream_->recv(&tag, packet, true);
    if (retval == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to receive broadcast stream acks\n");
        return STAT_MRNET_ERROR;
    }
    if (packet->unpack("%d", &retval) == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to unpack acknowledgement packet\n");
        return STAT_MRNET_ERROR;
    }
    if (retval != 0)
    {
        printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "%d daemons reported an error\n", retval);
        return STAT_DAEMON_ERROR;
    }
    
    /* Load the STAT filter into MRNet */
    filterId = network_->load_FilterFunc(filterPath_, "statMerge");
    if (filterId == -1)
    {
        printMsg(STAT_FILTERLOAD_ERROR, __FILE__, __LINE__, "load_FilterFunc() failure for path %s, function statMerge\n", filterPath_);
        return STAT_FILTERLOAD_ERROR;
    }
    
    /* Setup the STAT merge stream */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Creating MRNet stream with STAT filter\n");
    mergeStream_ = network_->new_Stream(broadcastCommunicator_, filterId, SFILTER_WAITFORALL);
    if (mergeStream_ == NULL)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to setup STAT merge stream\n");
        return STAT_MRNET_ERROR;
    }

    endTime.setTime();
    addPerfData("\tStream Creation and Filter Load Time", (endTime - startTime).getDoubleTime());
    printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "\tMRNet connection configured\n");
    isConnected_ = true;
    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Tool daemons launched and connected!\n");

    /* TODO: can use merge stream to send CP log dir... CPs pick up in filter on ack from BEs */

    /* Make sure all daemons have the same version number as the FE */
    statError = checkVersion();
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to validate version number with the daemons.  The STAT FrontEnd is at version %d %d %d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
        return statError;
    }

#ifdef STAT_FGFS
    startTime.setTime();
    /* Check for the FGFS filter path */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Setting up FGFS\n");
    if (fgfsFilterPath_ == NULL) 
    {
        printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "FGFS Filter path not set\n");
        return STAT_ARG_ERROR;
    }

    /* Load the FGFS upstream filter into MRNet */
    filterId = network_->load_FilterFunc(fgfsFilterPath_, FGFS_UP_FILTER_FN_NAME);
    if (filterId == -1)
    {
        printMsg(STAT_FILTERLOAD_ERROR, __FILE__, __LINE__, "load_FilterFunc() failure for path %s, function %s\n", fgfsFilterPath_, FGFS_UP_FILTER_FN_NAME);
        return STAT_FILTERLOAD_ERROR;
    }
    
    /* Load the FGFS downstream filter into MRNet */
    filterId2 = network_->load_FilterFunc(fgfsFilterPath_, FGFS_DOWN_FILTER_FN_NAME);
    if (filterId2 == -1)
    {
        printMsg(STAT_FILTERLOAD_ERROR, __FILE__, __LINE__, "load_FilterFunc() failure for path %s, function %s\n", fgfsFilterPath_, FGFS_DOWN_FILTER_FN_NAME);
        return STAT_FILTERLOAD_ERROR;
    }
    
    /* Setup the FGFS stream */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Creating MRNet stream with the FGFS filter\n");
    fgfsStream_ = network_->new_Stream(broadcastCommunicator_, filterId, SFILTER_WAITFORALL, filterId2);
    if (fgfsStream_ == NULL)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to setup FGFS stream\n");
        return STAT_MRNET_ERROR;
    }

    /* Send an empty message using the FGFS stream */
    if (fgfsStream_->send(PROT_SEND_FGFS_STREAM, "%auc", "x", 2) == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "failed to send on fgfs stream\n");
        return STAT_MRNET_ERROR;
    }
    if (fgfsStream_->flush() == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "failed to flush fgfs stream\n");
        return STAT_MRNET_ERROR;
    }

    /* Initialize the FGFS Comm Fabric */
    ret = MRNetCommFabric::initialize((void *)network_, (void *)fgfsStream_);
    if (ret == false)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to initialize FGFS CommFabric\n");
        return STAT_MRNET_ERROR;
    }
    fgfsCommFabric_ = new MRNetCommFabric();
    ret = AsyncGlobalFileStat::initialize(fgfsCommFabric_);
    if (ret == false)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to initialize AsyncGlobalFileStat\n");
        return STAT_MRNET_ERROR;
    }
    
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "FGFS setup complete\n");
    endTime.setTime();
    addPerfData("\tFGFS Setup Time", (endTime - startTime).getDoubleTime());

    /* send the file request stream to the BEs */
    startTime.setTime();
    statError = sendFileReqStream();
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "sendFileReqStream reported an error\n");
        return statError;
    }
    endTime.setTime();
    addPerfData("\tFile Broadcast Setup Time", (endTime - startTime).getDoubleTime());
#endif

    totalEnd.setTime();
    addPerfData("\tTotal MRNet Launch Time", (totalEnd - totalStart).getDoubleTime());

    /* If we're STATBench, then release the job launcher in case we need to debug */
    if (isStatBench)
    {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "STATBench releasing the job launcher\n");
        if (isLaunched_ == true)
        {
            lmon_rc_e rc = LMON_fe_detach(lmonSession_);
            if (rc != LMON_OK)
                printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Launchmon failed to detach from launcher... this probably means that the helper daemons exited normally\n");
            isLaunched_ = false;
        }
    }    
    
    return STAT_OK;
}

#ifdef STAT_FGFS
StatError_t STAT_FrontEnd::sendFileReqStream()
{
    int upstreamFileRequestFilterId, downstreamFileRequestFilterId, retval, tag;
    PacketPtr packet;
    StatError_t ret;

    if (isConnected_ == false)
    {
        printMsg(STAT_NOT_CONNECTED_ERROR, __FILE__, __LINE__, "STAT daemons have not been launched\n");
        return STAT_NOT_CONNECTED_ERROR;
    }
    upstreamFileRequestFilterId = network_->load_FilterFunc(filterPath_, "fileRequestUpStream");
    if (upstreamFileRequestFilterId == -1)
    {
        printMsg(STAT_FILTERLOAD_ERROR, __FILE__, __LINE__, "load_FilterFunc() failure\n");
        return STAT_FILTERLOAD_ERROR;
    }
  
    downstreamFileRequestFilterId = network_->load_FilterFunc(filterPath_, "fileRequestDownStream");
    if (downstreamFileRequestFilterId == -1)
    {      
        printMsg(STAT_FILTERLOAD_ERROR, __FILE__, __LINE__, "load_FilterFunc() failure\n");
        return STAT_FILTERLOAD_ERROR;
    }
  
    // Create a stream that will use the fileRequestUpStream and fileRequestDownStream filters for file requests
    fileRequestStream_ = network_->new_Stream(broadcastCommunicator_, upstreamFileRequestFilterId, SFILTER_DONTWAIT, downstreamFileRequestFilterId);
    if (fileRequestStream_ == NULL)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to get STAT file request stream\n");
        return STAT_MRNET_ERROR;
    }

    // Broadcast a message to back-ends regarding the Library File Request Stream
    if (fileRequestStream_->send(PROT_FILE_REQ, "%auc %s", "X", 2, "X") == -1 )
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to send file request stream message\n");
        return STAT_MRNET_ERROR;
    }
    if (fileRequestStream_->flush() == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to flush message\n");
        return STAT_MRNET_ERROR;
    }

    /* Receive acks from all BEs */
    pendingAckTag_ = PROT_FILE_REQ_RESP;
    isPendingAck_ = true;
    pendingAckCb_ = NULL;
    ret = receiveAck(true);
    if (ret != STAT_OK)
    {
        printMsg(ret, __FILE__, __LINE__, "Failed to receive file stream setup ack\n");
        return ret;
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Recieved acknowledgements from all BEs\n");
    return STAT_OK;
}

// KUMAR ADDED:  This function waits for BEs openSymbolReader messages and replies with its contents of library files
StatError_t STAT_FrontEnd::waitForFileRequests(unsigned int *streamId,
                                               int *returnTag,
                                               PacketPtr &packetPtr,
                                               int &retval)
{
    int tag, size;
    char *receiveFile;
    size_t pos;
    char *contents;
    FILE *fp;
    PacketPtr packet;
    Stream *stream;
    string filePath, fileName;

    while (1)
    {
        retval = network_->recv(&tag, packet, &stream, false); 
        if (retval == 0)
            return STAT_PENDING_ACK;
        else if (retval < 0)
        {
            printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "network::recv() failure\n");
            return STAT_MRNET_ERROR;
        }
                 
        if (tag != PROT_LIB_REQ)
        {    
            *streamId = stream->get_Id();
            *returnTag = tag;
            packetPtr = packet;
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "waitForFileRequests returing tag %d, stream ID %d\n", tag, streamId);
            return STAT_OK;
        }
        if (packet->unpack("%s", &receiveFile) == -1)
        {
            printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "packet::unpack() failure\n");
            return STAT_MRNET_ERROR;
        }
    
        filePath = receiveFile;
        pos = filePath.find_last_of("/");
        fileName = filePath.substr(pos + 1);

//    char objcopy_cache_dir[BUFSIZE];
//        char* cache_dir = getenv("STAT_OBJCOPY_CACHE_DIR");
//    if (cache_dir == NULL)
//    {
//        char* homeDirectory = getenv("HOME");
//        if (homeDirectory == NULL)
//        {
//        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, 
//             "STAT_OBJCOPY_CACHE_DIR not set in environment, and failed to get $HOME as an alternative.\n");
//        return STAT_FILE_ERROR;
//        }
//
//        snprintf(objcopy_cache_dir, BUFSIZE, "%s/STAT_objcopy_cache", homeDirectory);
//        int ret = mkdir(objcopy_cache_dir, S_IRUSR | S_IWUSR | S_IXUSR); 
//        if (ret == -1 && errno != EEXIST)
//        {
//            printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, 
//             "Failed to create objcopy cache directory at %s\n", objcopy_cache_dir);
//        return STAT_FILE_ERROR;
//        }
//    }
//    else
//    {
//        strncpy(objcopy_cache_dir, cache_dir, BUFSIZE);
//    }
//
//    std::string newfile(objcopy_cache_dir);
//    newfile.append(fileName);
//    int cpid = fork();
//    if (cpid < 0)
//        {
//        int err = errno;
//        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, 
//             "fork() for running objcopy failed with %s\n", strerror(err));
//        return STAT_SYSTEM_ERROR;
//    }
//    else if (cpid == 0)
//        {
//        char* objcopy_exe = "/usr/bin/objcopy";
//        //char* objcopy_exe = getenv("STAT_OBJCOPY_EXE");
//        //if (objcopy_exe == NULL)
//        //{
//        //    printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, 
//        //     "Failed to get STAT_OBJCOPY_EXE from environment\n");
//        //return STAT_FILE_ERROR;
//        //}
//
//        char* ex_args[14];
//        ex_args[0] = strdup(objcopy_exe);
//        ex_args[1]= "--only-section";
//        ex_args[2] = ".interp";
//        ex_args[3]= "--only-section" ;
//        ex_args[4]= ".dynamic";
//        ex_args[5]= "--only-section";
//        ex_args[6]= ".dynstr";
//        ex_args[7]= "--only-section";
//        ex_args[8]= ".dynsym";
//        ex_args[9]= "--only-section";
//        ex_args[10]= ".eh_frame";
//        ex_args[11]= "--only-keep-debug";
//        ex_args[12]= strdup(receiveFile);
//        ex_args[13]= strdup(newfile.c_str());
//        ex_args[14]= (char*) 0;
//        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, 
//             "execing objcopy to copy %s to %s\n",
//             receiveFile, newfile.c_str());
//        execv(ex_args[0],ex_args);
//    }
//    int objc = wait(NULL);

     
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "received request for file %s\n", receiveFile);

//        *fp = fopen(newfile.c_str(),"r");
        fp = fopen(filePath.c_str(), "r");
        if (fp == NULL)
        {
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "waitForFileRequests: file %s does not exist\n", filePath.c_str());
            size = 4;        
            contents = (char *)malloc(size * sizeof(char));
            if (contents == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: failed to malloc %d elements for contents\n", strerror(errno), size);
                return STAT_ALLOCATE_ERROR;
            }
            strcpy(contents, "err");
            tag = PROT_LIB_REQ_ERR;
        }
        else
        {            
            fseek(fp, 0, SEEK_END);
            size = ftell(fp);
            if (size == -1)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: ftell returned -1\n", strerror(errno), size);
                fclose(fp);
                return STAT_ALLOCATE_ERROR;
            }
            fseek(fp, 0, SEEK_SET);
            contents = (char *)malloc(size * sizeof(char));
            if (contents == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: failed to malloc %d elements for contents\n", strerror(errno), size);
                fclose(fp);
                return STAT_ALLOCATE_ERROR;
            }
            fread(contents, size, 1, fp);
            fclose(fp);
            tag = PROT_LIB_REQ_RESP;
        }
        if (stream->send(tag, "%ac %s", contents, size, receiveFile) == -1)
        {
            printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "failed to send file contents\n");
            return STAT_MRNET_ERROR;
        }
        if (stream->flush() == -1)
        {
            printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "failed to flush file contents\n");
            return STAT_MRNET_ERROR;
        }
    }
}

#endif //STAT_FGFS

char *STAT_FrontEnd::getLastErrorMessage()
{
    return lastErrorMessage_;
}

const char *STAT_FrontEnd::getInstallPrefix()
{
    char *envValue;
    envValue = getenv("STAT_PREFIX");
    if (envValue != NULL)
        return envValue;
    return STAT_PREFIX;
}

void STAT_FrontEnd::getVersion(int *version)
{
    version[0] = STAT_MAJOR_VERSION;
    version[1] = STAT_MINOR_VERSION;
    version[2] = STAT_REVISION_VERSION;
}

StatError_t STAT_FrontEnd::dumpProctab()
{
    int i;
    FILE *f;
    char fileName[BUFSIZE];

    /* Create the process table file */
    snprintf(fileName, BUFSIZE, "%s/%s.ptab", outDir_, filePrefix_);
    f = fopen(fileName, "w");
    if (f == NULL)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: fopen failed to create ptab file %s\n", strerror(errno), fileName);
        return STAT_FILE_ERROR;
    }

    /* Write the job launcher host and PID */
    if (remoteNode_ != NULL)
        fprintf(f, "%s:%d\n", remoteNode_, launcherPid_);
    else    
        fprintf(f, "%s:%d\n", hostname_, launcherPid_);

    /* Write out the MPI ranks */
    if (verbose_ == STAT_VERBOSE_FULL || logging_ & STAT_LOG_FE)
        printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "Process Table:\n");
    for (i = 0; i < nApplProcs_; i++)
    {
        fprintf(f, "%d %s:%d %s\n", proctab_[i].mpirank, proctab_[i].pd.host_name, proctab_[i].pd.pid, proctab_[i].pd.executable_name);
        if (verbose_ == STAT_VERBOSE_FULL || logging_ & STAT_LOG_FE)
            printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "%s: %s, pid: %d, MPI rank: %d\n", proctab_[i].pd.host_name, proctab_[i].pd.executable_name, proctab_[i].pd.pid, proctab_[i].mpirank);
    }

    fclose(f);
    return STAT_OK;
}

StatError_t STAT_FrontEnd::startLog(unsigned char logType, char *logOutDir)
{
    int ret;
    char fileName[BUFSIZE];

    logging_ = logType;
    snprintf(logOutDir_, BUFSIZE, "%s", logOutDir);
   
    /* Make sure the log directory name has been set */
    if (strcmp(logOutDir_, "NULL") == 0)
    {
        printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Can't start log.  Log output directory not specified\n");
        return STAT_ARG_ERROR;
    }
 
    /* Create the log directory */
    ret = mkdir(logOutDir_, S_IRUSR | S_IWUSR | S_IXUSR); 
    if (ret == -1 && errno != EEXIST)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: mkdir failed to create log directory %s\n", strerror(errno), logOutDir);
        return STAT_FILE_ERROR;
    }

    /* If we're logging the FE, open the log file */
    if (logging_ & STAT_LOG_FE)
    {
        snprintf(fileName, BUFSIZE, "%s/%s.STAT.log", logOutDir_, hostname_);
        statOutFp = fopen(fileName, "w");
        if (statOutFp == NULL)
        {
            printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: fopen failed to open FE log file %s\n", strerror(errno), fileName);
            return STAT_FILE_ERROR;
        }
        if (logging_ & STAT_LOG_MRN)
            mrn_printf_init(statOutFp);
    }

    return STAT_OK;
}

StatError_t STAT_FrontEnd::receiveAck(bool blocking)
{
    int tag = 0, retval;
    StatError_t statError;
    PacketPtr packet;
    
    if (isPendingAck_ == false)
        return STAT_OK;

    /* If we're pending stack traces, call the receive function */
    if (pendingAckTag_ == PROT_SEND_LAST_TRACE_RESP || pendingAckTag_ == PROT_SEND_TRACES_RESP)
    {
        statError = receiveStackTraces(blocking);
        if (statError == STAT_OK)
            return statError;
        else if (statError == STAT_PENDING_ACK)
            return statError;
        else 
        {
            printMsg(statError, __FILE__, __LINE__, "Failed to receive stack traces\n");
            isPendingAck_ = false;
            return statError;
        }
    }

    /* Receive an acknowledgement packet that all daemons have completed */
    do
    {
        if (hasFatalError_ == true)
        {
            printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "A Fatal Error has been detected\n");
            isPendingAck_ = false;
            return STAT_DAEMON_ERROR;
        }
#ifdef STAT_FGFS
        unsigned int streamId = 0;        
        statError = waitForFileRequests(&streamId, &tag, packet, retval);
        if (statError == STAT_PENDING_ACK)
        {
            if (blocking == true)
                usleep(1000);
            continue;
        }
        else if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Unable to process file requests\n");
            return statError;
        }
        if (streamId == broadcastStream_->get_Id())
        {
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received message on broadcast stream %d\n", broadcastStream_->get_Id());
#else
        retval = broadcastStream_->recv(&tag, packet, false);
#endif
        if (retval == 0)
        {
            if (!WIFBESPAWNED(lmonState))
            {
                printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
                isPendingAck_ = false;
                return STAT_DAEMON_ERROR;
            }
            if (blocking == true)
                usleep(1000);
        }
#ifdef STAT_FGFS
        }
#endif
    }
    while (retval == 0 && blocking == true);

    /* Check for errors */
    if (retval == 0 && blocking == false)
        return STAT_PENDING_ACK;
    else if (retval == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to receive acks\n");
        isPendingAck_ = false;
        return STAT_MRNET_ERROR;
    }
    if (tag != pendingAckTag_)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Received unexpected tag %d, expecting %d\n", tag, pendingAckTag_);
        isPendingAck_ = false;
        return STAT_MRNET_ERROR;
    }

    /* Unpack the ack and check for errors */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Successfully received ack with tag %d\n", tag);
    if (packet->unpack("%d", &retval) == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to unpack acknowledgement packet\n");
        isPendingAck_ = false;
        return STAT_MRNET_ERROR;
    }
    if (retval != 0)
    {
        printMsg(STAT_RESUME_ERROR, __FILE__, __LINE__, "%d daemons reported an error\n", retval);
        isPendingAck_ = false;
        return STAT_DAEMON_ERROR;
    }

    isPendingAck_ = false;
    if (pendingAckCb_ != NULL)
        (*this.*pendingAckCb_)();
    return STAT_OK;
}

StatError_t STAT_FrontEnd::setDaemonNodes()
{
    set<MRN::NetworkTopology::Node *> nodes;
    set<MRN::NetworkTopology::Node *>::iterator nodeIter;
    string prettyHost;

    /* Set the application node list */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Generating daemon node list: ");
    leafInfo_.networkTopology->get_BackEndNodes(nodes);
    if (nodes.size() <= 0)
    {
        printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "No daemons are connected\n");
        return STAT_DAEMON_ERROR;
    }
    leafInfo_.daemons.clear();
    for (nodeIter = nodes.begin(); nodeIter != nodes.end(); nodeIter++)
    {
        XPlat::NetUtils::GetHostName((*nodeIter)->get_HostName(), prettyHost);
        leafInfo_.daemons.insert(prettyHost);
        printMsg(STAT_LOG_MESSAGE, __FILE__, -1, "%s, ", prettyHost.c_str());
    }        
    printMsg(STAT_LOG_MESSAGE, __FILE__, -1, "\n");
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Daemon node list created\n");

    return STAT_OK;
}

StatError_t STAT_FrontEnd::setAppNodeList()
{
    unsigned int i;
    char *currentNode;

    /* Set the application node list */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Generating application node list: ");
    applicationNodeMultiSet_.clear();
    for (i = 0; i < nApplProcs_; i++)
    {
        currentNode = proctab_[i].pd.host_name;
        if (applicationNodeMultiSet_.find(currentNode) == applicationNodeMultiSet_.end())
        {
            applicationNodeMultiSet_.insert(currentNode);
            printMsg(STAT_LOG_MESSAGE, __FILE__, -1, "%s, ", currentNode);
        }
    }        
    printMsg(STAT_LOG_MESSAGE, __FILE__, -1, "\n");
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Application node list created\n");
    nApplNodes_ = applicationNodeMultiSet_.size();

    return STAT_OK;
}

StatError_t STAT_FrontEnd::createDaemonRankMap()
{
    unsigned int i, j;
    char *currentNode;
    IntList_t *daemonRanks;
    map<string, vector<int> > tempMap;
    map<string, vector<int> >::iterator iter;
    
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Creating daemon rank map\n");

    /* First, create a map containing all daemons, with the MPI ranks that each daemon debugs */
    for (i = 0; i < nApplProcs_; i++)
    {
        currentNode = proctab_[i].pd.host_name;
        tempMap[currentNode].push_back(proctab_[i].mpirank);
    }

    /* Next sort each daemon's rank list and store it in the IntList_t ranks map */
    j = 0;
    for (iter = tempMap.begin(); iter != tempMap.end(); iter++)
    {   
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Daemon %s, ranks:", iter->first.c_str());
        sort((iter->second).begin(), (iter->second).end());
        daemonRanks = (IntList_t *)malloc(sizeof(IntList_t));
        if (daemonRanks == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: malloc failed to allocate for daemonRanks\n", strerror(errno));
            return STAT_ALLOCATE_ERROR;
        }
        daemonRanks->count = (iter->second).size();
        daemonRanks->list = (int *)malloc(daemonRanks->count * sizeof(int));
        if (daemonRanks->list == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: malloc failed to allocate for daemonRanks->list\n", strerror(errno));
            return STAT_ALLOCATE_ERROR;
        }
        for (i = 0; i < (iter->second).size(); i++)
        {
            printMsg(STAT_LOG_MESSAGE, __FILE__, -1, "%d, ", (iter->second)[i]);
            daemonRanks->list[i] = (iter->second)[i];
        }
        mrnetRankToMPIRanksMap_[topologySize_ + j] = daemonRanks;
        printMsg(STAT_LOG_MESSAGE, __FILE__, -1, "\n");
        j++;
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Daemon rank map created\n");
    return STAT_OK;
}

StatError_t STAT_FrontEnd::setCommNodeList(char *nodeList, bool checkAccess = true)
{
    char numString[BUFSIZE], nodeName[BUFSIZE], *nodeRange;
    unsigned int num1 = 0, num2, startPos, endPos, i, j;
    bool isRange = false;
    string baseNodeName, nodes, list;
    string::size_type openBracketPos, closeBracketPos, commaPos, finalPos;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Generating communication node list\n");

    /* Make sure the node list has been set */
    if (nodeList == NULL)
    {
        /* There may be enough resources where the STAT FE is being run so 
           we'll return OK for now... we will check for sufficient resources 
           when we create the topology file anyway */
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "No nodes specified for communication processes\n");
        return STAT_OK;
    }
   
    list = nodeList;
    while (1)
    {
        openBracketPos = list.find_first_of("[");
        closeBracketPos = list.find_first_of("]");
        commaPos = list.find_first_of(",");

        if (openBracketPos == string::npos && commaPos == string::npos)
            finalPos = list.length(); /* Last one, just a single node */
        else if (commaPos < openBracketPos)
            finalPos = commaPos; /* just a single node */
        else  
            finalPos = closeBracketPos + 1;
        nodes = list.substr(0, finalPos);
        openBracketPos = nodes.find_first_of("[");
        closeBracketPos = nodes.find_first_of("]");

        if (openBracketPos == string::npos && closeBracketPos == string::npos)
        {
            /* This is a single node */
            strncpy(nodeName, nodes.c_str(), BUFSIZE);
            if (checkAccess == true)
            {
                if (checkNodeAccess(nodeName))
                {
                    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Node %s added to communication node list\n", nodeName);
                    communicationNodeSet_.insert(nodeName);
                }
                else
                    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Node %s not accessable\n", nodeName);
            }
            else
                communicationNodeSet_.insert(nodeName);
        }
        else 
        {
            /* This is a list of nodes */
            /* Parse the node list string string e.g.: xxx[0-15,12,23,26-35] */
            nodeRange = strdup(nodes.substr(openBracketPos + 1, closeBracketPos - (openBracketPos + 1)).c_str());
            if (nodeRange == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed on call to set node range with strdup()\n", strerror(errno));
                return STAT_ARG_ERROR;
            }

            /* Get the machine name */
            baseNodeName = nodes.substr(0, openBracketPos);

            /* Decode the node list string  */
            for (i = 0; i < strlen(nodeRange); i++)
            {
                if (nodeRange[i] == ',')
                    continue;
                else if (isdigit(nodeRange[i]))
                {
                    startPos = i;
                    while (isdigit(nodeRange[i]))
                    {
                        i++;
                        if (i >= strlen(nodeRange))
                            break;
                    }    
                    endPos = i - 1;
                }
                else
                {
                    printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Invalid node list %s\nEncountered unexpected character '%c'\n", nodeList, nodeRange[i]);
                    free(nodeRange);
                    return STAT_ARG_ERROR;
                }

                memcpy(numString, nodeRange + startPos, endPos-startPos + 1);
                numString[endPos-startPos + 1] = '\0';

                if (isRange)
                {
                    isRange = false;
                    num2 = atoi(numString);
                    for (j = num1; j <= num2; j++)
                    {
                        snprintf(nodeName, BUFSIZE, "%s%u", baseNodeName.c_str(), j);
                        if (checkAccess == true)
                        {
                            if (checkNodeAccess(nodeName))
                            {
                                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Node %s added to communication node list\n", nodeName);
                                communicationNodeSet_.insert(nodeName);
                            }
                            else
                                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Node %s not accessable\n", nodeName);
                        }
                        else
                            communicationNodeSet_.insert(nodeName);
                    }
                }
                else
                {
                    num1 = atoi(numString);
                    if (i < strlen(nodeRange))
                    {
                        if (nodeRange[i] == '-')
                        {
                            isRange = true;
                            continue;
                        }
                    }
                    snprintf(nodeName, BUFSIZE, "%s%u", baseNodeName.c_str(), num1);
                    if (checkAccess == true)
                    {
                        if (checkNodeAccess(nodeName))
                        {
                            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Node %s added to communication node list\n", nodeName);
                            communicationNodeSet_.insert(nodeName);
                        }
                        else
                            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Node %s not accessable\n", nodeName);
                        }
                    else
                        communicationNodeSet_.insert(nodeName);
                }
                i = i - 1;
            }

            if (nodeRange != NULL)
                free(nodeRange);
        }
        if (finalPos >= list.length())
            break;
        else    
            list = list.substr(finalPos + 1, list.length() - 1);
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Communication node list created with %d nodes\n", communicationNodeSet_.size());
    return STAT_OK;
}

StatError_t STAT_FrontEnd::createOutputDir()
{
    int ret, fileNameCount;
    char cwd[BUFSIZE], resultsDirectory[BUFSIZE], *homeDirectory;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Creating output directory\n");

    /* Look for the STAT_results directory and create if it doesn't exist */
    if (getcwd(cwd, BUFSIZE) == NULL)
    {
        printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "%s: getcwd failed\n", strerror(errno));
        return STAT_SYSTEM_ERROR;
    }        
    snprintf(resultsDirectory, BUFSIZE, "%s/STAT_results", cwd);
    ret = mkdir(resultsDirectory, S_IRUSR | S_IWUSR | S_IXUSR); 
    if (ret == -1 && errno != EEXIST)
    {
        /* User may not have write privileges in CWD, try the user's home directory */
        homeDirectory = getenv("HOME");
        if (homeDirectory == NULL)
        {
            printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "Failed to create output directory in current working directory and failed to get $HOME as an alternative.\n");
            return STAT_FILE_ERROR;
        }
        snprintf(resultsDirectory, BUFSIZE, "%s/STAT_results", homeDirectory);
        ret = mkdir(resultsDirectory, S_IRUSR | S_IWUSR | S_IXUSR); 
        if (ret == -1 && errno != EEXIST)
        {
            printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "Failed to create output directory in current working directory %s and failed to create output directory in $HOME %s as an alternative.\n", cwd, homeDirectory);
            return STAT_FILE_ERROR;
        }
    }

    /* Create run-specific results directory with a unique name */
    for (fileNameCount = 0; fileNameCount < STAT_MAX_FILENAME_ID; fileNameCount++)
    {
        if (jobId_ == 0)
        {
            if (fileNameCount == 0)
                snprintf(outDir_, BUFSIZE, "%s/%s", resultsDirectory, basename(applExe_));
            else
                snprintf(outDir_, BUFSIZE, "%s/%s.%04d", resultsDirectory, basename(applExe_), fileNameCount);
        }
        else
        {
            if (fileNameCount == 0)
                snprintf(outDir_, BUFSIZE, "%s/%s.%d", resultsDirectory, basename(applExe_), jobId_);
            else
                snprintf(outDir_, BUFSIZE, "%s/%s.%d.%04d", resultsDirectory, basename(applExe_), jobId_, fileNameCount);
        }
        ret = mkdir(outDir_, S_IRUSR | S_IWUSR | S_IXUSR);
        if (ret == 0)
            break;
        else if (ret == -1 && errno != EEXIST)
        {
            printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: mkdir failed to create run specific directory %s\n", strerror(errno), outDir_);
            return STAT_FILE_ERROR;
        }
    }
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Output directory %s created\n", outDir_);

    /* Generate the file prefix for output files */
    if (jobId_ == 0)
        snprintf(filePrefix_, BUFSIZE, "%s.%04d", basename(applExe_), fileNameCount);
    else
        snprintf(filePrefix_, BUFSIZE, "%s.%d.%04d", basename(applExe_), jobId_, fileNameCount);
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Generated file prefix: %s\n", outDir_);
    
    return STAT_OK;
}

StatError_t STAT_FrontEnd::createTopology(char *topologyFileName, StatTopology_t topologyType, char *topologySpecification, char *nodeList, bool shareAppNodes)
{
    FILE *file;
    char tmp[BUFSIZE], *topology = NULL;
    int parentCount, desiredDepth = 0, desiredMaxFanout = 0, procsNeeded = 0;
    unsigned int i, j, counter, layer, parentIter, childIter;
    unsigned int depth = 0, fanout = 0;
    vector<string> treeList;
    set<string>::iterator communicationNodeSetIter;
    multiset<string>::iterator applicationNodeMultiSetIter;
    string topoIter, current;
    string::size_type dashPos, lastPos;
    StatError_t statError;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Creating MRNet topology file\n");
   
    /* Set parameters based on requested topology */
    if (topologyType == STAT_TOPOLOGY_DEPTH)
    {
        desiredMaxFanout = STAT_MAX_FANOUT;
        desiredDepth = atoi(topologySpecification);
    }
    else if (topologyType == STAT_TOPOLOGY_FANOUT)
        desiredMaxFanout = atoi(topologySpecification);
    else if (topologyType == STAT_TOPOLOGY_USER)
        topology = strdup(topologySpecification);
    else
        desiredMaxFanout = STAT_MAX_FANOUT;

    /* Set the communication node list if we're not using a flat 1 to N tree */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Setting communication node list\n");
    if ((desiredMaxFanout < nApplProcs_ && desiredMaxFanout > 0) || topology != NULL || desiredDepth != 0)
    {
        /* Add application nodes to list if requested */
        if (shareAppNodes == true)
        {
#ifdef BGL
            printMsg(STAT_WARNING, __FILE__, __LINE__, "Sharing of application nodes not supported on BlueGene systems\n");
#else
            char *appNodes;
            int size = BUFSIZE;
            appNodes = (char *)malloc(BUFSIZE);
            if (appNodes == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to alloc appNodes\n", strerror(errno));
                return STAT_ALLOCATE_ERROR;
            }
            sprintf(appNodes, "");
            for (applicationNodeMultiSetIter = applicationNodeMultiSet_.begin(); applicationNodeMultiSetIter != applicationNodeMultiSet_.end(); applicationNodeMultiSetIter++) 
            {
                if (strlen((*applicationNodeMultiSetIter).c_str()) + strlen(appNodes) >= size)
                {
                    size += BUFSIZE;
                    appNodes = (char *)realloc(appNodes, size);
                    if (appNodes == NULL)
                    {
                        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to realloc appNodes to size %d\n", strerror(errno), size);
                        return STAT_ALLOCATE_ERROR;
                    }
                }
                strncat(appNodes, (*applicationNodeMultiSetIter).c_str(), size - 1);
                strncat(appNodes, ",", size - 1);
            }
            appNodes[strlen(appNodes) - 1] = '\0'; /* to remove last comma */
            statError = setCommNodeList(appNodes, false);
            if (statError != STAT_OK)
            {
                printMsg(statError, __FILE__, __LINE__, "Failed to set the global node list\n");
                return statError;
            }
#endif
        }
        if (communicationNodeSet_.size() == 0)
        {
            if (nodeList == NULL)
            {
                statError = setNodeListFromConfigFile(&nodeList);
                if (statError != STAT_OK)
                    printMsg(statError, __FILE__, __LINE__, "Failed to get node list from config file\n");
            }
            else
            {
                if (strcmp(nodeList, "") == 0)
                {
                    statError = setNodeListFromConfigFile(&nodeList);
                    if (statError != STAT_OK)
                        printMsg(statError, __FILE__, __LINE__, "Failed to get node list from config file\n");
                }
            }
    
            statError = setCommNodeList(nodeList);
            if (statError != STAT_OK)
            {
                printMsg(statError, __FILE__, __LINE__, "Failed to set the global node list\n");
                return statError;
            }
        }
    }


    /* Set the requested topology and check if there are enough CPs specified */
    if (topology == NULL)
    {
        /* Determine the depth and fanout */
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Setting topology depth %d fanout %d node count %d\n", desiredDepth, desiredMaxFanout, nApplNodes_);
    
        if (desiredDepth == 0)
        {
            /* Find the desired depth based on the fanout and number of app nodes */
            for (desiredDepth = 1; desiredDepth < 1024; desiredDepth++)
            {
                fanout = (int)ceil(pow((float)nApplNodes_, (float)1.0 / (float)desiredDepth));
                if (fanout <= desiredMaxFanout)
                    break;
            }
        }
        else
            fanout = (int)ceil(pow((float)nApplNodes_, (float)1.0 / (float)desiredDepth));

        /* Determine the number of processes needed */
        procsNeeded = 0;
        for (i = 1; i < desiredDepth; i++)
        {
            if (i == 1)
            {
                topology = (char *)malloc(BUFSIZE);
                if (topology == NULL)
                {
                    printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: malloc failed to allocate for topology\n", strerror(errno));
                    return STAT_ALLOCATE_ERROR;
                }
                snprintf(topology, BUFSIZE, "%d", fanout);
            }
            else
            {
                snprintf(tmp, BUFSIZE, "-%d", (int)ceil(pow((float)fanout, (float)i)));
                strcat(topology, tmp);
            }
            procsNeeded += (int)ceil(pow((float)fanout, (float)i));
        }
        if (procsNeeded <= communicationNodeSet_.size() * procsPerNode_)
        {
            /* We have enough CPs, so we can have our desired depth */
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Requested topology set with depth %d, fanout %d\n", desiredDepth, fanout);
        }
        else
        {
            /* There aren't enough CPs, so make a 2-deep tree with as many CPs as we have */
            if (communicationNodeSet_.size() == 0)
                printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "Not enough processes for specified topology.  %d processes needed for depth of %d and fanout of %d.  Reverting to flat topology\n", procsNeeded, desiredDepth, fanout);
            else
            {
                if (topologyType == STAT_TOPOLOGY_AUTO)
                    printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "Not enough processes for automatic topology.  %d processes needed for depth of %d and fanout of %d.  Reverting to tree with one layer of %d communication processes\n", procsNeeded, desiredDepth, fanout, communicationNodeSet_.size() * procsPerNode_);
                else
                    printMsg(STAT_WARNING, __FILE__, __LINE__, "Not enough processes specified for the requested topology depth %d, fanout %d: %d processes needed, %d processes specified.  Reverting to tree with one layer of %d communication processes.  Next time, please specify more resources with --nodes and --procs or request the use of application nodes with --appnodes.\n", desiredDepth, fanout, procsNeeded, communicationNodeSet_.size() * procsPerNode_, communicationNodeSet_.size() * procsPerNode_);
            }
            topology = (char *)malloc(BUFSIZE);
            if (topology == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: malloc failed to allocate for topology\n", strerror(errno));
                return STAT_ALLOCATE_ERROR;
            }
            snprintf(topology, BUFSIZE, "%d", (int)communicationNodeSet_.size() * procsPerNode_);
        }
    }
    else
    {
        procsNeeded = 0;
        topoIter = topology;
        while(1)
        {
            dashPos = topoIter.find_first_of("-");
            if (dashPos == string::npos)
                lastPos = topoIter.length();
            else
                lastPos = dashPos;    
            current = topoIter.substr(0, lastPos);
            layer = atoi(current.c_str());
            procsNeeded += layer;
            if (lastPos >= topoIter.length())
                break;
            topoIter = topoIter.substr(lastPos + 1);
        }
        if (procsNeeded > communicationNodeSet_.size() * procsPerNode_)
        {
            printMsg(STAT_WARNING, __FILE__, __LINE__, "Not enough processes specified for the requested topology %s: %d processes needed, %d processes specified.  Reverting to tree with one layer of %d communication processes.  Next time, please specify more resources with --nodes and --procs.\n", topology, procsNeeded, communicationNodeSet_.size() * procsPerNode_, communicationNodeSet_.size() * procsPerNode_);
            if (topology != NULL)
                free(topology);
            topology = (char *)malloc(BUFSIZE);
            if (topology == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: malloc failed to allocate for topology\n", strerror(errno));
                return STAT_ALLOCATE_ERROR;
            }
            snprintf(topology, BUFSIZE, "%d", (int)communicationNodeSet_.size() * procsPerNode_);
        }
    }

    /* Check if tool FE hostname is in application list and the communication 
       node list, then we will later add it to the comm nodes */

    /* Add the FE to the root of the tree */
#ifdef BGL
    /* On BlueGene systems we need the network interface with the IO nodes */
    snprintf(tmp, BUFSIZE, "%s-io", hostname_);
    snprintf(hostname_, BUFSIZE, "%s", tmp);
#endif
    snprintf(tmp, BUFSIZE, "%s:0", hostname_);
    treeList.push_back(tmp);

    /* Make sure the procsPerNode_ is set */
    if (procsPerNode_ <= 0)
    {
        printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "At least one process must be allowed per node... %d specified\n", procsPerNode_);
        return STAT_ARG_ERROR;
    }

    /* Add the nodes and IDs to the list of hosts */
    counter = 0;
    for (i = 0; i < procsPerNode_; i++)
    {
        for (communicationNodeSetIter = communicationNodeSet_.begin(); communicationNodeSetIter != communicationNodeSet_.end(); communicationNodeSetIter++)
        {
            counter++;
            if ((*communicationNodeSetIter) == hostname_)
                snprintf(tmp, BUFSIZE, "%s:%d", (*communicationNodeSetIter).c_str(), i + 1);
            else
                snprintf(tmp, BUFSIZE, "%s:%d", (*communicationNodeSetIter).c_str(), i);
            treeList.push_back(tmp);
        }
    }    

    /* Create the topology file */
    snprintf(topologyFileName, BUFSIZE, "%s/%s.top", outDir_, filePrefix_);
    file = fopen(topologyFileName, "w");
    if (file == NULL)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: fopen failed to create topology file %s\n", strerror(errno), topologyFileName);
        return STAT_FILE_ERROR;
    }
   
    /* Initialized vector iterators */
    if (topology == NULL) /* Flat topology */
        fprintf(file, "%s;\n", treeList[0].c_str());
    else if (strcmp(topology, "0") == 0) /* Flat topology */
        fprintf(file, "%s;\n", treeList[0].c_str());
    else
    {
        /* Create the topology file from specification */
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Generating specified topology\n");
        topoIter = topology;
        parentIter = 0;
        parentCount = 1;
        childIter = 1;

        /* Parse the specification and create the topology */
        while(true)
        {
            dashPos = topoIter.find_first_of("-");
            if (dashPos == string::npos)
                lastPos = topoIter.length();
            else
                lastPos = dashPos;    
            current = topoIter.substr(0, lastPos);
            layer = atoi(current.c_str());

            /* Loop around the parent's for this layer */
            for (i = 0; i < parentCount; i++)
            {
                if (parentIter >= treeList.size())
                {
                    printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Not enough resources for specified topology.  Please specify more resources with --nodes and --procs.\n");
                    fclose(file);
                    return STAT_ARG_ERROR;
                }
                fprintf(file, "%s =>", treeList[parentIter].c_str());

                /* Add the children for this layer */
                for (j = 0; j < (layer / parentCount) + (layer % parentCount > i ? 1 : 0); j++)
                {
                    if (childIter >= treeList.size())
                    {
                        printMsg(STAT_ARG_ERROR, __FILE__, __LINE__, "Not enough resources for specified topology.  Please specify more resources with --nodes and --procs.\n");
                        fclose(file);
                        return STAT_ARG_ERROR;
                    }
                    fprintf(file, "\n\t%s", treeList[childIter].c_str());
                    childIter++;
                }
                fprintf(file, ";\n\n");
                parentIter++;
            }

            parentCount = layer;
            if (lastPos >= topoIter.length())
                break;
            topoIter = topoIter.substr(lastPos + 1);
        }
    }
    
    fclose(file);
    if (topology != NULL)
        free(topology);
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "MRNet topology file created\n");
    return STAT_OK;
}

StatError_t STAT_FrontEnd::checkVersion()
{
    int filterId, tag, daemonCount, filterCount, retval;
    int major, minor, revision;
    Stream *stream;
    PacketPtr packet;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Comparing STAT FE version %d.%d.%d with the daemons\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);

    if (isConnected_ == false)
    {
        printMsg(STAT_NOT_CONNECTED_ERROR, __FILE__, __LINE__, "STAT daemons have not been launched\n");
        return STAT_NOT_CONNECTED_ERROR;
    }

    /* Load the STAT version check filter into MRNet */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Setting up version check filter\n");
    filterId = network_->load_FilterFunc(filterPath_, "STAT_checkVersion");
    if (filterId == -1)
    {
        printMsg(STAT_FILTERLOAD_ERROR, __FILE__, __LINE__, "load_FilterFunc() failure\n");
        return STAT_FILTERLOAD_ERROR;
    }

    /* Setup the STAT version check stream */
    stream = network_->new_Stream(broadcastCommunicator_, filterId, SFILTER_WAITFORALL);
    if (stream == NULL)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to get STAT version check stream\n");
        return STAT_MRNET_ERROR;
    }

    /* Send request to daemons to check version and wait for reply */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Sending version message to daemons\n");
    if (stream->send(PROT_CHECK_VERSION, "%d %d %d", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION) == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to send version check message\n");
        return STAT_MRNET_ERROR;
    }
    if (stream->flush() != 0)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to flush message\n");
        return STAT_MRNET_ERROR;
    }

    /* Receive a single acknowledgement packet */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Receiving version check acknowledgements\n");
    do
    {
        retval = stream->recv(&tag, packet, false);
        if (retval == 0)
        {
            if (!WIFBESPAWNED(lmonState))
            {
                printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
                return STAT_DAEMON_ERROR;
            }
            usleep(1000);
        }
    }
    while (retval == 0);
    if (retval == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to receive version message acks\n");
        return STAT_MRNET_ERROR;
    }
    if (tag != PROT_CHECK_VERSION_RESP)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Unexpected tag %d, expecting PROT_CHECK_VERSION_RESP = %d\n", tag, PROT_CHECK_VERSION_RESP);
        return STAT_MRNET_ERROR;
    }

    /* Unpack the ack and check for errors */
    if (packet->unpack("%d %d %d %d %d", &major, &minor, &revision, &daemonCount, &filterCount) == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to unpack acknowledgement packet\n");
        return STAT_MRNET_ERROR;
    }

    /* Look for any version mismatches */
    if (filterCount != 0 || daemonCount != 0)
    {
        if (filterCount != 0)
            printMsg(STAT_VERSION_ERROR, __FILE__, __LINE__, "%d filters reported a version mismatch\n", filterCount);
        if (daemonCount != 0)
            printMsg(STAT_VERSION_ERROR, __FILE__, __LINE__, "%d daemons reported a version mismatch\n", daemonCount);
        return STAT_VERSION_ERROR;
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "All daemon versions are compatible\n");

    return STAT_OK;
}

StatError_t STAT_FrontEnd::attachApplication(bool blocking)
{
    int tag, retval;
    StatError_t ret;
    PacketPtr packet;

    startTime.setTime();

    /* Make sure we're in the expected state */
    if (isAttached_ == true)
    {
        printMsg(STAT_STDOUT, __FILE__, __LINE__, "STAT already attached to the application... ignoring request to attach\n");
        return STAT_OK;
    }
    if (isConnected_ == false)
    {
        printMsg(STAT_NOT_CONNECTED_ERROR, __FILE__, __LINE__, "STAT daemons have not been launched\n");
        return STAT_NOT_CONNECTED_ERROR;
    }
    if (WIFKILLED(lmonState))
    {
        printMsg(STAT_APPLICATION_EXITED, __FILE__, __LINE__, "LMON detected the application has exited\n");
        return STAT_APPLICATION_EXITED;
    }
    if (!WIFBESPAWNED(lmonState))
    {
        printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
        return STAT_DAEMON_ERROR;
    }

    /* Check for pending acks */
    ret = receiveAck(true);
    if (ret != STAT_OK)
    {
        printMsg(ret, __FILE__, __LINE__, "Failed to receive pending ack, attach canceled\n");
        return ret;
    }

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Attaching to application...\n");

    /* Send request to daemons to attach to application and wait for reply */
    if (broadcastStream_->send(PROT_ATTACH_APPLICATION, "") == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to send attach request\n");
        return STAT_MRNET_ERROR;
    }
    if (broadcastStream_->flush() != 0)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to flush message\n");
        return STAT_MRNET_ERROR;
    }

    pendingAckTag_ = PROT_ATTACH_APPLICATION_RESP;
    isPendingAck_ = true;
    pendingAckCb_ = &STAT_FrontEnd::postAttachApplication;
    if (blocking == true)
    {
        ret = receiveAck(true);
        if (ret != STAT_OK)
        {
            printMsg(ret, __FILE__, __LINE__, "Failed to receive attach ack\n");
            return ret;
        }
    }
    return STAT_OK;
}

StatError_t STAT_FrontEnd::postAttachApplication()
{
    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Attached!\n");
    isAttached_ = true;
    isRunning_ = false;

    endTime.setTime();
    addPerfData("Attach Time", (endTime - startTime).getDoubleTime());
    return STAT_OK;
}

StatError_t STAT_FrontEnd::pause(bool blocking)
{
    int tag, retval;
    StatError_t ret;
    PacketPtr packet;

    startTime.setTime();

    /* Make sure we're in the expected state */
    if (isRunning_ == false)
    {
        printMsg(STAT_STDOUT, __FILE__, __LINE__, "Application already paused... ignoring request to pause\n");
        return STAT_OK;
    }
    if (isAttached_ == false)
    {
        printMsg(STAT_STDOUT, __FILE__, __LINE__, "STAT not attached to the application\n");
        return STAT_NOT_ATTACHED_ERROR;
    }
    if (isConnected_ == false)
    {
        printMsg(STAT_NOT_CONNECTED_ERROR, __FILE__, __LINE__, "STAT daemons have not been launched\n");
        return STAT_NOT_CONNECTED_ERROR;
    }
    if (WIFKILLED(lmonState))
    {
        printMsg(STAT_APPLICATION_EXITED, __FILE__, __LINE__, "LMON detected the application has exited\n");
        return STAT_APPLICATION_EXITED;
    }
    if (!WIFBESPAWNED(lmonState))
    {
        printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
        return STAT_DAEMON_ERROR;
    }

    /* Check for pending acks */
    ret = receiveAck(true);
    if (ret != STAT_OK)
    {
        printMsg(ret, __FILE__, __LINE__, "Failed to receive pending ack, pause canceled\n");
        return ret;
    }

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Pausing the application...\n");

    /* Send request to daemons to pause the application and wait for reply */
    if (broadcastStream_->send(PROT_PAUSE_APPLICATION, "") == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to send pause request\n");
        return STAT_MRNET_ERROR;
    }
    if (broadcastStream_->flush() != 0)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to flush message\n");
        return STAT_MRNET_ERROR;
    }

    pendingAckTag_ = PROT_PAUSE_APPLICATION_RESP;
    isPendingAck_ = true;
    pendingAckCb_ = &STAT_FrontEnd::postPauseApplication;
    if (blocking == true)
    {
        ret = receiveAck(blocking);
        if (ret != STAT_OK)
        {
            printMsg(ret, __FILE__, __LINE__, "Failed to receive pause ack\n");
            return ret;
        }
    }
    return STAT_OK;
}

StatError_t STAT_FrontEnd::postPauseApplication()
{
    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Paused!\n");
    isRunning_ = false;
    endTime.setTime();
    addPerfData("Pause Time", (endTime - startTime).getDoubleTime());
    return STAT_OK;
}

StatError_t STAT_FrontEnd::resume(bool blocking)
{
    int tag, retval;
    StatError_t ret;
    PacketPtr packet;

    startTime.setTime();

    /* Make sure we're in the expected state */
    if (isRunning_ == true)
    {
        printMsg(STAT_STDOUT, __FILE__, __LINE__, "Application already running... ignoring request to resume\n");
        return STAT_OK;
    }
    if (isAttached_ == false)
    {
        printMsg(STAT_STDOUT, __FILE__, __LINE__, "STAT not attached to the application\n");
        return STAT_NOT_ATTACHED_ERROR;
    }
    if (isConnected_ == false)
    {
        printMsg(STAT_NOT_CONNECTED_ERROR, __FILE__, __LINE__, "STAT daemons have not been launched\n");
        return STAT_NOT_CONNECTED_ERROR;
    }
    if (WIFKILLED(lmonState))
    {
        printMsg(STAT_APPLICATION_EXITED, __FILE__, __LINE__, "LMON detected the application has exited\n");
        return STAT_APPLICATION_EXITED;
    }
    if (!WIFBESPAWNED(lmonState))
    {
        printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
        return STAT_DAEMON_ERROR;
    }

    /* Check for pending acks */
    ret = receiveAck(true);
    if (ret != STAT_OK)
    {
        printMsg(ret, __FILE__, __LINE__, "Failed to receive pending ack, resume canceled\n");
        return ret;
    }

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Resuming the application...\n");

    /* Send request to daemons to resume the application and wait for reply */
    if (broadcastStream_->send(PROT_RESUME_APPLICATION, "") == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to send resume request\n");
        return STAT_MRNET_ERROR;
    }
    if (broadcastStream_->flush() != 0)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to flush message\n");
        return STAT_MRNET_ERROR;
    }

    pendingAckTag_ = PROT_RESUME_APPLICATION_RESP;
    isPendingAck_ = true;
    pendingAckCb_ = &STAT_FrontEnd::postResumeApplication;
    if (blocking == true)
    {
        ret = receiveAck(blocking);
        if (ret != STAT_OK)
        {
            printMsg(ret, __FILE__, __LINE__, "Failed to receive resume ack\n");
            return ret;
        }
    }
    return STAT_OK;
}

StatError_t STAT_FrontEnd::postResumeApplication()
{
    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Resumed!\n");
    isRunning_ = true;
    endTime.setTime();
    addPerfData("Resume Time", (endTime - startTime).getDoubleTime());
    return STAT_OK;
}

bool STAT_FrontEnd::isRunning()
{
    return isRunning_;
}

StatError_t STAT_FrontEnd::sampleStackTraces(StatSample_t sampleType, bool withThreads, bool clearOnSample, unsigned int nTraces, unsigned int traceFrequency, unsigned int nRetries, unsigned int retryFrequency, bool blocking, char *variableSpecification)
{
    int tag, retval;
    StatError_t ret;
    PacketPtr packet;

    startTime.setTime();

    /* Make sure we're in the expected state */
    if (isAttached_ == false)
    {
        printMsg(STAT_NOT_ATTACHED_ERROR, __FILE__, __LINE__, "STAT not attached to the application... ignoring request to gather samples\n");
        return STAT_NOT_ATTACHED_ERROR;
    }
    if (isConnected_ == false)
    {
        printMsg(STAT_NOT_CONNECTED_ERROR, __FILE__, __LINE__, "STAT daemons have not been launched\n");
        return STAT_NOT_CONNECTED_ERROR;
    }
    if (WIFKILLED(lmonState))
    {
        printMsg(STAT_APPLICATION_EXITED, __FILE__, __LINE__, "LMON detected the application has exited\n");
        return STAT_APPLICATION_EXITED;
    }
    if (!WIFBESPAWNED(lmonState))
    {
        printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
        return STAT_DAEMON_ERROR;
    }

    /* Check for pending acks */
    ret = receiveAck(true);
    if (ret != STAT_OK)
    {
        printMsg(ret, __FILE__, __LINE__, "Failed to receive pending ack, sample canceled\n");
        return ret;
    }

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Sampling traces...\n");
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "%d traces with %d frequency, %d retries with %d frequency\n", nTraces, traceFrequency, nRetries, retryFrequency);

    /* Send request to daemons to gather stack traces and wait for confirmation */
    if (broadcastStream_->send(PROT_SAMPLE_TRACES, "%ud %ud %ud %ud %ud %ud %ud %s", nTraces, traceFrequency, nRetries, retryFrequency, sampleType, withThreads, clearOnSample, variableSpecification) == -1) 
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to send request to sample\n");
        return STAT_MRNET_ERROR;
    }
    if (broadcastStream_->flush() != 0)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to flush message\n");
        return STAT_MRNET_ERROR;
    }
   
    pendingAckTag_ = PROT_SAMPLE_TRACES_RESP;
    isPendingAck_ = true;
    pendingAckCb_ = &STAT_FrontEnd::postSampleStackTraces;
    if (blocking == true)
    {
        ret = receiveAck(blocking);
        if (ret != STAT_OK)
        {
            printMsg(ret, __FILE__, __LINE__, "Failed to receive stack samples\n");
            return ret;
        }
    }
    return STAT_OK;
}

StatError_t STAT_FrontEnd::postSampleStackTraces()
{
    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Traces sampled!\n");
    endTime.setTime();
    addPerfData("Sample Traces Time", (endTime - startTime).getDoubleTime());
    return STAT_OK;
}

StatError_t STAT_FrontEnd::gatherLastTrace(bool blocking)
{
    StatError_t statError;

    /* Check for pending acks */
    statError = receiveAck(true);
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to receive pending ack, gather canceled\n");
        return statError;
    }

    statError = gatherImpl(PROT_SEND_LAST_TRACE, blocking);
    return statError;
}

StatError_t STAT_FrontEnd::gatherTraces(bool blocking)
{
    StatError_t statError;

    /* Check for pending acks */
    statError = receiveAck(true);
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to receive pending ack, gather canceled\n");
        return statError;
    }

    statError = gatherImpl(PROT_SEND_TRACES, blocking);
    return statError;
}

StatError_t STAT_FrontEnd::gatherImpl(StatProt_t type, bool blocking)
{
    StatError_t statError = STAT_OK;

    /* Make sure we're in the expected state */
    if (isConnected_ == false)
    {
        printMsg(STAT_NOT_CONNECTED_ERROR, __FILE__, __LINE__, "STAT daemons have not been launched\n");
        return STAT_NOT_CONNECTED_ERROR;
    }
    if (!WIFBESPAWNED(lmonState))
    {
        printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
        return STAT_DAEMON_ERROR;
    }

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Merging traces...\n");

    /* Gather merged traces from the daemons */
    startTime.setTime();

    /* Make sure we're in the expected state */
    if (isConnected_ == false)
    {
        printMsg(STAT_NOT_CONNECTED_ERROR, __FILE__, __LINE__, "STAT daemons have not been launched\n");
        return STAT_NOT_CONNECTED_ERROR;
    }
    if (!WIFBESPAWNED(lmonState))
    {
        printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
        return STAT_DAEMON_ERROR;
    }

    /* Send request to daemons to send merged stack traces */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Sending request to daemons to send traces\n");
    if (mergeStream_->send(type, "") == -1) 
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to send request to gather traces\n");
        return STAT_MRNET_ERROR;
    }
    if (mergeStream_->flush() != 0)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to flush message\n");
        return STAT_MRNET_ERROR;
    }

    isPendingAck_ = true;
    if (type == PROT_SEND_LAST_TRACE)
        pendingAckTag_ = PROT_SEND_LAST_TRACE_RESP;
    else
        pendingAckTag_ = PROT_SEND_TRACES_RESP;

    if (blocking == true)
    {
        statError = receiveStackTraces(true);
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Failed to receive attach ack\n");
            return statError;
        }
    }

    return STAT_OK;
}

StatError_t STAT_FrontEnd::receiveStackTraces(bool blocking)
{
    static int mergeCount2d = -1, mergeCount3d = -1;
    int tag, totalWidth, retval, dummyRank, offset, mergeCount;
    unsigned int byteArrayLen;
    char outFile[BUFSIZE], perfData[BUFSIZE], outSuffix[BUFSIZE], *byteArray;
    list<int>::iterator ranksIter;
    graphlib_graph_p stackTraces, sortedStackTraces;
    graphlib_error_t gl_err;
    IntList_t *hostRanks;
    PacketPtr packet;

    /* Receive the traces */
    do
    {
        retval = mergeStream_->recv(&tag, packet, false);
        if (retval == 0)
        {
            if (!WIFBESPAWNED(lmonState))
            {
                printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
                return STAT_DAEMON_ERROR;
            }
            if (blocking == true)
                usleep(1000);
        }
    }
    while (retval == 0 && blocking == true);

    /* Check for errors */
    if (retval == 0 && blocking == false)
        return STAT_PENDING_ACK;
    else if (retval == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to gather stack traces\n");
        isPendingAck_ = false;
        return STAT_MRNET_ERROR;
    }

    if (tag == PROT_SEND_LAST_TRACE_RESP)
    {
        mergeCount2d++;
        mergeCount = mergeCount2d;
        snprintf(outSuffix, BUFSIZE, "2D");
    }    
    else
    {
        mergeCount3d++;
        mergeCount = mergeCount3d;
        snprintf(outSuffix, BUFSIZE, "3D");
    }    
    snprintf(perfData, BUFSIZE, "Gather %s Traces Time (receive and merge)", outSuffix);
    addPerfData(perfData, -1.0);
    isPendingAck_ = false;

    /* Unpack byte array representing the graph */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Unpacking traces\n");
    if (packet->unpack("%ac %d %d", &byteArray, &byteArrayLen, &totalWidth, &dummyRank) == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::unpack(PROT_COLLECT_TRACES_RESP, \"%%auc\") failed\n");
        return STAT_MRNET_ERROR;
    }

    /* Initialize graphlib */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Initializing graphlib\n");
    gl_err = graphlib_InitVarEdgeLabels(totalWidth);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to initialize graphlib\n");
        return STAT_GRAPHLIB_ERROR;
    }

    /* Deserialize graph */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Deserializing graph 1\n");
    gl_err = graphlib_deserializeGraph(&stackTraces, byteArray, byteArrayLen);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "deserializeGraph() failed\n");
        return STAT_GRAPHLIB_ERROR;
    }

    endTime.setTime();
    addPerfData("\tMerge", (endTime - startTime).getDoubleTime());

    /* Translate the graphs into rank ordered bit vector */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Creating new graphs\n");
    startTime.setTime();
    offset = 0;
    gl_err = graphlib_newGraph(&sortedStackTraces);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to create new graph\n");
        return STAT_GRAPHLIB_ERROR;
    }
    
    /* Copy the unordered graphs, but with empty bit vectors */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Copying graph with empty edges\n");
    gl_err = graphlib_mergeGraphsEmptyEdges(sortedStackTraces, stackTraces);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to merge graph with empty edge labels\n");
        return STAT_GRAPHLIB_ERROR;
    }

    /* Fill edge labels on a per daemon basis */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Filling in edges\n");
    for (ranksIter = remapRanksList_.begin(); ranksIter != remapRanksList_.end(); ranksIter++)
    {
        /* Fill edge labels for this daemon */
        hostRanks = mrnetRankToMPIRanksMap_[*ranksIter];
        gl_err = graphlib_mergeGraphsFillEdges(sortedStackTraces, stackTraces, hostRanks->list, hostRanks->count, offset);
        if (GRL_IS_FATALERROR(gl_err))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to fill edge labels\n");
            return STAT_GRAPHLIB_ERROR;
        }
       
        /* update offset, round up to the nearest bit vector count*/
        offset += hostRanks->count / (graphlib_getBitVectorSize() * 8);
        if (hostRanks->count % (graphlib_getBitVectorSize() * 8) != 0)
            offset += 1;
    }
    
    endTime.setTime();
    addPerfData("Graph Rank Ordering Time", (endTime - startTime).getDoubleTime());

    /* Export temporally and spatially merged graph */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Exporting %s graph to dot\n", outSuffix);
    startTime.setTime();

    /* Dump spatial-temporally merged graph to dot format */
    gl_err = graphlib_colorGraphByLeadingEdgeLabel(sortedStackTraces);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "graphlib error coloring graph by leading edge label\n");
        return STAT_GRAPHLIB_ERROR;
    }        
    gl_err = graphlib_scaleNodeWidth(sortedStackTraces, 80, 160);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "graphlib error scaling node width\n");
        return STAT_GRAPHLIB_ERROR;
    }
    if (mergeCount == 0)
        snprintf(outFile, BUFSIZE, "%s/%s.%s.dot", outDir_, filePrefix_, outSuffix);
    else
        snprintf(outFile, BUFSIZE, "%s/%s_%d.%s.dot", outDir_, filePrefix_, mergeCount, outSuffix);
    snprintf(lastDotFileName_, BUFSIZE, "%s", outFile);
    gl_err = graphlib_exportGraph(outFile, GRF_DOT, sortedStackTraces);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "graphlib error exporting graph to dot format\n");
        return STAT_GRAPHLIB_ERROR;
    }        

    endTime.setTime();
    addPerfData("Export Graph Files Time", (endTime - startTime).getDoubleTime());
    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Traces merged!\n");

    /* Delete the graphs */
    gl_err = graphlib_delGraph(stackTraces);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
        return STAT_GRAPHLIB_ERROR;
    }
    gl_err = graphlib_delGraph(sortedStackTraces);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
        return STAT_GRAPHLIB_ERROR;
    }

    return STAT_OK;
}

char *STAT_FrontEnd::getLastDotFilename()
{
    return lastDotFileName_;
}

StatError_t STAT_FrontEnd::addPerfData(const char *buf, double time)
{
    pair<string, double> perfData(buf, time);
    performanceData_.push_back(perfData);
    return STAT_OK;
}

StatError_t STAT_FrontEnd::dumpPerf()
{
    char perfFileName[BUFSIZE], usageLogFile[BUFSIZE];
    FILE *perfFile;
    static int count = 0;
    int i, size;
    bool isUsageLogging = false;

    /* Exit with OK if there is no data to dump */
    size = performanceData_.size();
    if (size <= 0)
        return STAT_OK;

    if (strcmp(outDir_, "NULL") == 0)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "Output directory not created.  Performance results not written.\n");
        return STAT_FILE_ERROR;
    }

    /* Create performance results file */
    snprintf(perfFileName, BUFSIZE, "%s/%s.perf", outDir_, filePrefix_);
    perfFile = fopen(perfFileName, "a");
    if (perfFile ==NULL)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: fopen failed to create performance results file\n", strerror(errno));
        return STAT_FILE_ERROR;
    }
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Dumping performance info to %s\n", perfFileName);

    /* Dump header info only on first invocation */
    if (count == 0)
    {
        fprintf(perfFile, "STAT version %d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
        fprintf(perfFile, "Application: %s\n", applExe_);
        fprintf(perfFile, "Number of Nodes: %d\n", nApplNodes_);
        fprintf(perfFile, "Number of Processes: %d\n\n", nApplProcs_);
        fprintf(perfFile, "All times reported in seconds\n\n");
        printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "Performance Results:\n");
        printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "STAT version %d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
        printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "Application: %s\n", applExe_);
        printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "Number of Nodes: %d\n", nApplNodes_);
        printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "Number of Processes: %d\n\n", nApplProcs_);
        printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "All times reported in seconds\n\n");
    }
    count++;

    /* Write out the results in the list */
    for (i = 0; i < size; i++)
    {
        if (performanceData_[i].second != -1.0)
            fprintf(perfFile, "%s: %.3lf\n", performanceData_[i].first.c_str(), performanceData_[i].second);
        else
            fprintf(perfFile, "%s:\n", performanceData_[i].first.c_str());
    }
    fclose(perfFile);
    if (verbose_ == STAT_VERBOSE_FULL || logging_ & STAT_LOG_FE)
    {
        for (i = 0; i < size; i++)
        {
            if (performanceData_[i].second != -1.0)
                printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "%s: %.3lf\n", performanceData_[i].first.c_str(), performanceData_[i].second);
            else
                printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "%s:\n", performanceData_[i].first.c_str());
        }
    }

#ifdef STAT_USAGELOG
    snprintf(usageLogFile , BUFSIZE, STAT_USAGELOG);
    double launchTime = -1.0, attachTime = -1.0, sampleTime = -1.0, mergeTime = -1.0, orderTime = -1.0, exportTime = -1.0;
    struct timeval timeStamp;
    time_t currentTime;
    char timeBuf[BUFSIZE], *userName;
    for (i = 0; i < size; i++)
    {
        if (performanceData_[i].first.find("Total MRNet Launch Time") != string::npos)
            launchTime = performanceData_[i].second;
        else if (performanceData_[i].first.find("Attach Time") != string::npos)
            attachTime = performanceData_[i].second;
        else if (performanceData_[i].first.find("Sample") != string::npos && sampleTime == -1.0)
            sampleTime = performanceData_[i].second;
        else if (performanceData_[i].first.find("Merge") != string::npos && mergeTime == -1.0)
            mergeTime = performanceData_[i].second;
        else if (performanceData_[i].first.find("Ordering") != string::npos && orderTime == -1.0)
            orderTime = performanceData_[i].second;
        else if (performanceData_[i].first.find("Export") != string::npos && exportTime == -1.0)
            exportTime = performanceData_[i].second;
    }
    perfFile = fopen(usageLogFile, "a");
    if (perfFile == NULL)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: fopen failed to open usage log file %s\n", strerror(errno), usageLogFile);
        return STAT_FILE_ERROR;
    }
    gettimeofday(&timeStamp, NULL);
    currentTime = timeStamp.tv_sec;
    strftime(timeBuf, BUFSIZE, "%Y-%m-%d-%T", localtime(&currentTime));
    userName = getlogin();
    if (userName == NULL)
    {
        userName = getenv("USER");
        if (userName == NULL)
            userName = "NULL";
            
    }
    fprintf(perfFile, "%s %s %s %d.%d.%d %d %d %.3lf %.3lf %.3lf %.3lf %.3lf %.3lf\n", timeBuf, hostname_, userName, STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION, nApplNodes_, nApplProcs_, launchTime, attachTime, sampleTime, mergeTime, orderTime, exportTime);
    fclose(perfFile);
#endif

    performanceData_.clear();

    return STAT_OK;
}

void STAT_FrontEnd::shutDown()
{
    int killed, detached;
    StatError_t statError;
    lmon_rc_e rc;

    /* Dump the performance metrics to a file */
    statError = dumpPerf();
    if (statError != STAT_OK)
        printMsg(statError, __FILE__, __LINE__, "Failed to dump performance results\n");

    /* Shut down the MRNet Tree */
    if (network_ != NULL && isConnected_ == true)
        shutdownMrnetTree();

    /* Close the LaunchMON session */
    killed = WIFKILLED(lmonState);
    detached = !WIFBESPAWNED(lmonState);
    if (isLaunched_ == true && killed == 0 && detached == 0)
    {
        rc = LMON_fe_detach(lmonSession_);
        if (rc != LMON_OK)
            printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Launchmon failed to detach from launcher... have the daemons exited?\n");
    }
}

StatError_t STAT_FrontEnd::detachApplication(bool blocking)
{
    return detachApplication(NULL, 0, blocking);
}

// TODO: the stop list obviously won't scale, but most subsets should be small anyway
StatError_t STAT_FrontEnd::detachApplication(int *stopList, int stopListSize, bool blocking)
{
    int tag, retval;
    StatError_t ret;
    PacketPtr packet;

    /* Make sure we're in the expected state */
    if (isAttached_ == false)
    {
        printMsg(STAT_STDOUT, __FILE__, __LINE__, "STAT not attached to the application... ignoring request to detach\n");
        return STAT_NOT_ATTACHED_ERROR;
    }
    if (isConnected_ == false)
    {
        printMsg(STAT_NOT_CONNECTED_ERROR, __FILE__, __LINE__, "STAT daemons have not been launched\n");
        return STAT_NOT_CONNECTED_ERROR;
    }
    if (WIFKILLED(lmonState))
    {
        printMsg(STAT_APPLICATION_EXITED, __FILE__, __LINE__, "LMON detected the application has exited\n");
        return STAT_APPLICATION_EXITED;
    }
    if (!WIFBESPAWNED(lmonState))
    {
        printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
        return STAT_DAEMON_ERROR;
    }

    /* Check for pending acks */
    ret = receiveAck(true);
    if (ret != STAT_OK)
    {
        printMsg(ret, __FILE__, __LINE__, "Failed to receive pending ack, detach canceled\n");
        return ret;
    }

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Detaching from application...\n");

    /* Send request to daemons to detach from application and wait for reply */
    if (broadcastStream_->send(PROT_DETACH_APPLICATION, "%aud", stopList, stopListSize) == -1) 
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to send detach request\n");
        return STAT_MRNET_ERROR;
    }
    if (broadcastStream_->flush() != 0)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to flush message\n");
        return STAT_MRNET_ERROR;
    }
    
    pendingAckTag_ = PROT_DETACH_APPLICATION_RESP;
    isPendingAck_ = true;
    pendingAckCb_ = &STAT_FrontEnd::postDetachApplication;
    if (blocking == true)
    {
        ret = receiveAck(blocking);
        if (ret != STAT_OK)
        {
            printMsg(ret, __FILE__, __LINE__, "Failed to receive detach ack\n");
            return ret;
        }
    }
    return STAT_OK;
}

StatError_t STAT_FrontEnd::postDetachApplication()
{
    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Detached!\n");
    isAttached_ = false;
    isRunning_ = false;
    return STAT_OK;
}

StatError_t STAT_FrontEnd::shutdownMrnetTree()
{
    if (broadcastStream_->send(PROT_EXIT, "") == -1) 
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to send exit message\n");
        return STAT_MRNET_ERROR;
    }
    if (broadcastStream_->flush() != 0)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to flush message\n");
        return STAT_MRNET_ERROR;
    }

#ifndef MRNET22
    network_->shutdown_Network();
#endif

    return STAT_OK;
}

void STAT_FrontEnd::printMsg(StatError_t statError, const char *sourceFile, int sourceLine, const char *fmt, ...)
{
    va_list arg;
    char timeString[BUFSIZE];
    const char *timeFormat = "%b %d %T";
    time_t currentTime;
    struct tm *localTime;

    /* If this is a log message and we're not logging, then return */
    if (statError == STAT_LOG_MESSAGE && !(logging_ & STAT_LOG_FE))
        return;

    /* Get the time */
    currentTime = time(NULL);
    localTime = localtime(&currentTime);
    if (localTime == NULL)
        snprintf(timeString, BUFSIZE, "NULL");
    else
        strftime(timeString, BUFSIZE, timeFormat, localTime);

    if (statError != STAT_LOG_MESSAGE && statError != STAT_STDOUT && statError != STAT_VERBOSITY)
    {
        /* Print the error to the screen (or designated error file) */
        fprintf(stderr, "<%s> <%s:% 4d> ", timeString, sourceFile, sourceLine);
        fprintf(stderr, "STAT returned error type ");
        statPrintErrorType(statError, stderr);
        fprintf(stderr, ": ");
        va_start(arg, fmt);
        vsnprintf(lastErrorMessage_, BUFSIZE, fmt, arg);
        va_end(arg);
        fprintf(stderr, "%s", lastErrorMessage_);
    }
    else if ((statError == STAT_STDOUT && verbose_ >= STAT_VERBOSE_STDOUT) || (statError == STAT_VERBOSITY && verbose_ == STAT_VERBOSE_FULL))
    {
        /* Print the message to stdout */
        va_start(arg, fmt);
        vfprintf(stdout, fmt, arg);
        va_end(arg);
        fflush(stdout);
    }
   
    /* Print the message to the log */
    if (logging_ & STAT_LOG_FE && statOutFp != NULL)
    {
        if (logging_ & STAT_LOG_MRN)
        {
            char msg[BUFSIZE];
            va_start(arg, fmt);
            vsnprintf(msg, BUFSIZE, fmt, arg);
            va_end(arg);
            mrn_printf(sourceFile, sourceLine, "", statOutFp, "%s", msg);
        }
        else
        {
            if (sourceLine != -1 && sourceFile != NULL)
                fprintf(statOutFp, "<%s> <%s:%d> ", timeString, sourceFile, sourceLine);
            if (statError != STAT_LOG_MESSAGE && statError != STAT_STDOUT && statError != STAT_VERBOSITY)
            {
                fprintf(statOutFp, "STAT returned error type ");
                statPrintErrorType(statError, statOutFp);
                fprintf(statOutFp, ": ");
            }    
            va_start(arg, fmt);
            vfprintf(statOutFp, fmt, arg);
            va_end(arg);
            fflush(statOutFp);
        }
    }
}

StatError_t STAT_FrontEnd::terminateApplication(bool blocking)
{
    int tag, retval;
    StatError_t ret;
    PacketPtr packet;

    /* Make sure we're in the expected state */
    if (isAttached_ == false)
    {
        printMsg(STAT_NOT_ATTACHED_ERROR, __FILE__, __LINE__, "STAT not attached to the application... ignoring request to terminate\n");
        return STAT_NOT_ATTACHED_ERROR;
    }
    if (isConnected_ == false)
    {
        printMsg(STAT_NOT_CONNECTED_ERROR, __FILE__, __LINE__, "STAT daemons have not been launched\n");
        return STAT_NOT_CONNECTED_ERROR;
    }
    if (WIFKILLED(lmonState))
    {
        printMsg(STAT_APPLICATION_EXITED, __FILE__, __LINE__, "LMON detected the application has exited\n");
        return STAT_APPLICATION_EXITED;
    }
    if (!WIFBESPAWNED(lmonState))
    {
        printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
        return STAT_DAEMON_ERROR;
    }

    /* Check for pending acks */
    ret = receiveAck(true);
    if (ret != STAT_OK)
    {
        printMsg(ret, __FILE__, __LINE__, "Failed to receive pending ack, terminate canceled\n");
        return ret;
    }

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Terminating Application ...");

    /* Send request to daemons to terminate application and wait for reply */
    if (broadcastStream_->send(PROT_TERMINATE_APPLICATION, "") == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to send terminate request\n");
        return STAT_MRNET_ERROR;
    }
    if (broadcastStream_->flush() != 0)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to flush message\n");
        return STAT_MRNET_ERROR;
    }
    
    pendingAckTag_ = PROT_TERMINATE_APPLICATION_RESP;
    isPendingAck_ = true;
    pendingAckCb_ = &STAT_FrontEnd::postTerminateApplication;
    if (blocking == true)
    {
        ret = receiveAck(blocking);
        if (ret != STAT_OK)
        {
            printMsg(ret, __FILE__, __LINE__, "Failed to receive terminate ack\n");
            return ret;
        }
    }
    return STAT_OK;
}

StatError_t STAT_FrontEnd::postTerminateApplication()
{
    isRunning_ = false;
    isAttached_ = false;
    printMsg(STAT_STDOUT, __FILE__, __LINE__, "\tApplication terminated!\n");
    return STAT_OK;
}

void STAT_FrontEnd::printCommunicationNodeList()
{
    set<string>::iterator iter;

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "\tCommunication Node List: ");
    for (iter = communicationNodeSet_.begin(); iter != communicationNodeSet_.end(); iter++) 
        printMsg(STAT_STDOUT, __FILE__, __LINE__, "%s, ", iter->c_str());
    printMsg(STAT_STDOUT, __FILE__, __LINE__, "\n");
}

void STAT_FrontEnd::printApplicationNodeList()
{
    multiset<string>::iterator iter;

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "\tApplication Node List: ");
    for (iter = applicationNodeMultiSet_.begin(); iter != applicationNodeMultiSet_.end(); iter++) 
        printMsg(STAT_STDOUT, __FILE__, __LINE__, "%s, ", iter->c_str());
    printMsg(STAT_STDOUT, __FILE__, __LINE__, "\n");
}

StatError_t STAT_FrontEnd::setNodeListFromConfigFile(char **nodeList)
{
    FILE *f;
    char fileName[BUFSIZE], input[BUFSIZE];
    string nodes;
    int count = 0;

    snprintf(fileName, BUFSIZE, "%s/etc/STAT/nodes.txt", getInstallPrefix());
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Setting node list from file %s\n", fileName);
    f = fopen(fileName, "r");
    if (f != NULL)
    {
        while (fscanf(f, "%s", input) != EOF)
        {
            if (input[0] == '#')
            {
                while (1)
                {
                    int ch = fgetc(f);
                    if ((char)ch == '\n' || ch == EOF)
                        break;
                }
                continue;
            }
            if (count != 0)
                nodes += ",";
            nodes += input;
            count++;
        }
        *nodeList = strdup(nodes.c_str());
        fclose(f);
    }
    else
        printMsg(STAT_VERBOSITY, __FILE__, __LINE__, "No config file %s\n", fileName);

    return STAT_OK;
}

unsigned int STAT_FrontEnd::getLauncherPid()
{
    return launcherPid_;
}

unsigned int STAT_FrontEnd::getNumApplProcs()
{
    return nApplProcs_;
}

unsigned int STAT_FrontEnd::getNumApplNodes()
{
    return nApplNodes_;
}

void STAT_FrontEnd::setJobId(unsigned int jobId)
{
    jobId_ = jobId;
}


unsigned int STAT_FrontEnd::getJobId()
{
    return jobId_;
}


const char *STAT_FrontEnd::getApplExe()
{
    return applExe_;
}

StatError_t STAT_FrontEnd::setToolDaemonExe(const char *toolDaemonExe)
{
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Setting tool daemon path to %s\n", toolDaemonExe);
    if (toolDaemonExe_ != NULL)
        free(toolDaemonExe_);
    toolDaemonExe_ = strdup(toolDaemonExe);
    if (toolDaemonExe_ == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to set tool daemon path with strdup() to %s\n", strerror(errno), toolDaemonExe);
        return STAT_ALLOCATE_ERROR;
    }
    return STAT_OK;
}

const char *STAT_FrontEnd::getToolDaemonExe()
{
    return toolDaemonExe_;
}

StatError_t STAT_FrontEnd::setOutDir(const char *outDir)
{
    snprintf(outDir_, BUFSIZE, "%s", outDir);
    return STAT_OK;
}

const char *STAT_FrontEnd::getOutDir()
{
    return outDir_;
}

StatError_t STAT_FrontEnd::setFilePrefix(const char *filePrefix)
{
    snprintf(filePrefix_, BUFSIZE, "%s", filePrefix);
    return STAT_OK;
}

const char *STAT_FrontEnd::getFilePrefix()
{
    return filePrefix_;
}

void STAT_FrontEnd::setProcsPerNode(unsigned int procsPerNode)
{
    procsPerNode_ = procsPerNode;
}

unsigned int STAT_FrontEnd::getProcsPerNode()
{
    return procsPerNode_;
}

StatError_t STAT_FrontEnd::setFilterPath(const char *filterPath)
{
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Setting filter path to %s\n", filterPath);
    if (filterPath_ != NULL)
        free(filterPath_);
    filterPath_ = strdup(filterPath);
    if (filterPath_ == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to set filter path with strdup() to %s\n", strerror(errno), filterPath_);
        return STAT_ALLOCATE_ERROR;
    }
    return STAT_OK;
}    

const char *STAT_FrontEnd::getFilterPath()
{
    return filterPath_;
}

const char *STAT_FrontEnd::getRemoteNode()
{
    return remoteNode_;
}

StatError_t STAT_FrontEnd::addLauncherArgv(const char *launcherArg)
{
    launcherArgc_++;
    launcherArgv_ = (char **)realloc(launcherArgv_, launcherArgc_ * sizeof(char *));
    if (launcherArgv_ == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to realloc launcherArgv_ to %d\n", strerror(errno), launcherArgc_);
        return STAT_ALLOCATE_ERROR;
    }
    launcherArgv_[launcherArgc_ - 2] = strdup(launcherArg);
    if (launcherArgv_[launcherArgc_ - 2] == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to copy arg to launcherArgv_ with strdup() to %s\n", strerror(errno), launcherArg);
        return STAT_ALLOCATE_ERROR;
    }

    /* NULL terminate */
    launcherArgv_[launcherArgc_ - 1] = NULL;

    return STAT_OK;
}

const char **STAT_FrontEnd::getLauncherArgv()
{
    return (const char **)launcherArgv_;
}

unsigned int STAT_FrontEnd::getLauncherArgc()
{
    return launcherArgc_;
}
        
void STAT_FrontEnd::setVerbose(StatVerbose_t verbose)
{
    verbose_ = verbose;
}
        
StatVerbose_t STAT_FrontEnd::getVerbose()
{
    return verbose_;
}

#if (defined(HAVE_GETRLIMIT) && defined(HAVE_SETRLIMIT))
StatError_t increaseSysLimits()
{
    struct rlimit rlim[1];

    if (getrlimit(RLIMIT_NOFILE, rlim) < 0) 
    {
        fprintf(stderr, "%s: getrlimit failed\n", strerror(errno));
        return STAT_SYSTEM_ERROR;
    }
    else if (rlim->rlim_cur < rlim->rlim_max) 
    {
        rlim->rlim_cur = rlim->rlim_max;
        if (setrlimit (RLIMIT_NOFILE, rlim) < 0)
        {
            fprintf(stderr, "%s: Unable to increase max no. files\n", strerror(errno));
            return STAT_SYSTEM_ERROR;
        }    
    }

    if (getrlimit(RLIMIT_NPROC, rlim) < 0) 
    {
        fprintf(stderr, "%s: getrlimit failed\n", strerror(errno));
        return STAT_SYSTEM_ERROR;
    }
    else if (rlim->rlim_cur < rlim->rlim_max) 
    {
        rlim->rlim_cur = rlim->rlim_max;
        if (setrlimit (RLIMIT_NPROC, rlim) < 0)
        {
            fprintf(stderr, "%s: Unable to increase max no. files\n", strerror(errno));
            return STAT_SYSTEM_ERROR;
        }
    }

    return STAT_OK;
}
#endif

StatError_t STAT_FrontEnd::setRanksList()
{
    int i, j, rank;
    map<int, RemapNode_t*> childOrder;
    map<int, RemapNode_t*>::iterator childOrderIter;
    list<int>::iterator remapRanksListIter;
    RemapNode_t *root, *currentNode;
    StatError_t ret;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Creating the merge order ranks list\n");

    /* First we need to generate the list of STAT BE daemons */
    ret = setDaemonNodes();
    if (ret != STAT_OK)
    {
        printMsg(ret, __FILE__, __LINE__, "Failed to set daemon nodes\n");
        return ret;
    }

    /* Generate the rank tree */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Building the MRNet topology rank tree\n");
    root = buildRemapTree(leafInfo_.networkTopology->get_Root());
    if (root == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to Build Remap Tree\n");
        return STAT_ALLOCATE_ERROR;
    }

    /* Build the ordered ranks list based on the rank tree */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Generating the ranks list based on the rank tree\n");
    remapRanksList_.clear();
    ret = buildRanksList(root);
    if (ret != STAT_OK)
    {
        freeRemapTree(root);
        printMsg(ret, __FILE__, __LINE__, "Failed to build ranks list\n");
        return ret;
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Ranks list generated:");
    for (remapRanksListIter = remapRanksList_.begin(); remapRanksListIter != remapRanksList_.end(); remapRanksListIter++)
        printMsg(STAT_LOG_MESSAGE, __FILE__, -1, "%d, ", *remapRanksListIter);
    printMsg(STAT_LOG_MESSAGE, __FILE__, -1, "\n");
    
    freeRemapTree(root);

    return STAT_OK;
}

void STAT_FrontEnd::setHasFatalError(bool hasFatalError)
{
    hasFatalError_ = hasFatalError;
}

RemapNode_t *STAT_FrontEnd::buildRemapTree(NetworkTopology::Node *node)
{
    int i;
    set<NetworkTopology::Node *>::iterator iter;
    set<NetworkTopology::Node *> children;
    map<int, RemapNode_t *> childOrder;
    map<int, RemapNode_t *>::iterator childOrderIter;
    RemapNode_t *ret, *child;
   
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Remap tree MRNet rank %d\n", node->get_Rank());
    ret = (RemapNode_t *)malloc(sizeof(RemapNode_t));
    if (ret == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: malloc failed to allocate for ret\n", strerror(errno));
        return NULL;
    }
    ret->numChildren = 0;
    ret->children = NULL;
    ret->lowRank = -1;
    if (mrnetRankToMPIRanksMap_.find(node->get_Rank()) != mrnetRankToMPIRanksMap_.end())
    {
        /* This is a BE so return its rank */
        ret->lowRank = node->get_Rank();
    }
    else
    {
        /* This is not a BE so find the lowest rank among this node's children */
        children = node->get_Children();
        i = -1;
        for (iter = children.begin(); iter != children.end(); iter++)
        {
            i++;
            child = buildRemapTree(*iter);
            if (child == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to Build Remap Tree\n");
                return NULL;
            }
            if (i == 0)
                ret->lowRank = child->lowRank;
            else if (child->lowRank < ret->lowRank)
                ret->lowRank = child->lowRank;
            childOrder[child->lowRank] = child;
        }

        /* Add the children for this node, ordered by lowest rank */
        ret->numChildren = childOrder.size();
        if (ret->numChildren != 0)
        {
            ret->children = (RemapNode_t **)malloc(ret->numChildren * sizeof(RemapNode_t));
            if (ret->children == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: malloc failed to allocate for ret->children\n", strerror(errno));
                return NULL;
            }
            i = -1;
            for (childOrderIter = childOrder.begin(); childOrderIter != childOrder.end(); childOrderIter++)
            {
                i++;
                ret->children[i] = childOrderIter->second;
            }
        }
    }
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Remap tree MRNet rank %d with %d children and low rank %d\n", node->get_Rank(), ret->numChildren, ret->lowRank);
    return ret;
}

StatError_t STAT_FrontEnd::buildRanksList(RemapNode_t *node)
{
    int i;

    if (node->numChildren != 0)
    {
        /* Not a daemon so traverse children in rank order */
        for (i = 0; i < node->numChildren; i++)
            buildRanksList(node->children[i]);
    }
    else
    {
        /* If this is a daemon then add its rank to the remap ranks list */
        if (mrnetRankToMPIRanksMap_.find(node->lowRank) != mrnetRankToMPIRanksMap_.end())
            remapRanksList_.push_back(node->lowRank);
    }

    return STAT_OK;
}

StatError_t STAT_FrontEnd::freeRemapTree(RemapNode_t *node)
{
    int i;

    if (node->numChildren != 0)
    {
        /* Not a daemon so traverse children in rank order */
        for (i = 0; i < node->numChildren; i++)
            freeRemapTree(node->children[i]);
        if (node->children != NULL)
            free(node->children);
        if (node != NULL)
            free(node);
    }
    else
    {
        /* This is a daemon so just free it up */
        if (node != NULL)
            free(node);
    }

    return STAT_OK;
}

StatError_t STAT_FrontEnd::sendDaemonInfo()
{
    lmon_rc_e rc;
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Sending MRNet connection info to daemons\n");
    rc = LMON_fe_sendUsrDataBe(lmonSession_, (void *)&leafInfo_);
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to send data to BE\n");
        return STAT_LMON_ERROR;
    }
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "MRNet connection info successfully sent to daemons\n");

    return STAT_OK;
}

int statPack(void *data, void *buf, int bufMax, int *bufLen)
{
    int i, j, total = 0, nLeaves, len, len2, port, rank, parentPort, daemonCount, daemonRank;
    char *ptr = (char *)buf, *daemonCountPtr, *childCountPtr;
    unsigned int nNodes, depth, minFanout;
    unsigned maxFanout;
    double averageFanout, stdDevFanout;
    LeafInfo_t *leafInfo = (LeafInfo_t *)data;
    NetworkTopology *networkTopology = leafInfo->networkTopology;
    NetworkTopology::Node *node;
    set<string>::iterator daemonIter;
    set<NetworkTopology::Node *>::iterator iter;
    string currentHost;

    /* Get the Network Topology tree statisctics (we really just want # nodes */
    networkTopology->get_TreeStatistics(nNodes, depth, minFanout, maxFanout, averageFanout, stdDevFanout);

    /* reserve space for the number of daemons (to be calculated later) */
    daemonCountPtr = ptr;
    ptr += sizeof(int);
    total += sizeof(int);
  
    /* pack up the number of parent nodes */
    nLeaves = leafInfo->leafCps.size();
    memcpy(ptr, (void *)&nLeaves, sizeof(int));
    ptr += sizeof(int);
    total += sizeof(int);

    /* write the data one parent at a time */
    daemonCount = 0;
    daemonIter = leafInfo->daemons.begin();
    for (i = 0; i < nLeaves; i++)
    {
        /* get the parent info */
        node = leafInfo->leafCps[i];
        port = node->get_Port();
        currentHost = node->get_HostName();
        rank = node->get_Rank();

        /* calculate the amount of data */
        len = strlen(currentHost.c_str()) + 1;
        total += 3 * sizeof(int) + len;
        if (total > bufMax)
        {
            fprintf(stderr, "Exceeded maximum packing buffer\n");
            return -1;
        }

        /* write the parent host name, port, rank and child count */
        memcpy(ptr, (void *)currentHost.c_str(), len);
        ptr += len;
        memcpy(ptr, (void *)&port, sizeof(int));
        ptr += sizeof(int);
        memcpy(ptr, (void *)&rank, sizeof(int));
        ptr += sizeof(int);
        childCountPtr = ptr;
        ptr += sizeof(int);

        for (j = 0; j < (leafInfo->daemons.size() / nLeaves) + (leafInfo->daemons.size() % nLeaves > i ? 1 : 0); j++)
        {
            if (daemonIter == leafInfo->daemons.end())
                break;
            len = strlen(daemonIter->c_str()) + 1;
            total += sizeof(int) + len;

            if (total > bufMax)
            {
                fprintf(stderr, "Exceeded maximum packing buffer\n");
                return -1;
            }
            
            /* copy the daemon host name */
            memcpy(ptr, (void *)(daemonIter->c_str()), len);
            ptr += len;
            
            /* copy the daemon rank */
            daemonRank = daemonCount + nNodes;
            memcpy(ptr, (void *)(&daemonRank), sizeof(int));
            ptr += sizeof(int);
            daemonIter++;
            daemonCount++;
        }
        memcpy(childCountPtr, (void *)&j, sizeof(int));
    }

    /* write the daemon count to the appropriate location */
    memcpy(daemonCountPtr, (void *)&daemonCount, sizeof(int));

    *bufLen = total;
    return 0;
}


int lmonStatusCb(int *status)
{
    lmonState = *status;
    return 0;
}

bool STAT_FrontEnd::checkNodeAccess(char *node)
{
#ifdef CRAYXT
    /* MRNet CPs launched through alps */
    return true;
#else
    FILE *output, *temp;
    char command[BUFSIZE], testOutput[BUFSIZE], runScript[BUFSIZE], checkHost[BUFSIZE];
    char *rsh;

    /* TODO: this is generally not good... a quick check for clusters where each node has the same hostname prefix */
    /* A first level filter, compare the first 3 letters */
    gethostname(checkHost, BUFSIZE);
    if (strncmp(node, checkHost, 3) != 0)
        return false;

    rsh = getenv("XPLAT_RSH");
    if (rsh == NULL)
        rsh = "rsh";

    /* Use run_one_sec script if available to tolerate rsh slowness/hangs */
    snprintf(runScript, BUFSIZE, "%s/bin/run_one_sec", getInstallPrefix());
    temp = fopen(runScript, "r");
    if (temp == NULL)
        snprintf(command, BUFSIZE, "%s %s hostname 2>1", rsh, node);
    else
    {
        snprintf(command, BUFSIZE, "%s %s %s hostname 2>1", runScript, rsh, node);
        fclose(temp);
    }    
    output = popen(command, "r");
    if (output == NULL)
        return false;
    snprintf(testOutput, BUFSIZE, "");        
    fgets(testOutput, sizeof(testOutput), output);
    pclose(output);
    if (strcmp(testOutput, "") == 0)
        return false;
    return true;
#endif
}


/**********************
 * STATBench Routines
**********************/ 

StatError_t STAT_FrontEnd::STATBench_setAppNodeList()
{
    unsigned int i;
    char *currentNode;
    set<string>::iterator iter;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Generating application node list\n");
    applicationNodeMultiSet_.clear();

    for (i = 0; i < nApplProcs_; i++)
    {
        currentNode = proctab_[i].pd.host_name;
        applicationNodeMultiSet_.insert(currentNode);
    }
    nApplNodes_ = nApplProcs_;

    return STAT_OK;
}

StatError_t STAT_FrontEnd::statBenchCreateStackTraces(unsigned int maxDepth, unsigned int nTasks, unsigned int nTraces, unsigned int functionFanout, int nEqClasses)
{
    int tag, retval;
    char *currentNode;
    char name[BUFSIZE];
    unsigned int i, j;
    StatError_t statError;
    PacketPtr packet;

    addPerfData("STATBench Setup Time", -1.0);
    totalStart.setTime();

    /* Rework the proc table to represent the STATBench emulated application */
    startTime.setTime();

    for (i = 0; i < proctabSize_; i++)
    {
        if (proctab_[i].pd.executable_name != NULL)
            free(proctab_[i].pd.executable_name);
        if (proctab_[i].pd.host_name != NULL)
            free(proctab_[i].pd.host_name);
    }
    free(proctab_);
    proctab_ = NULL;

    nApplProcs_ = nApplNodes_ * nTasks;
    proctabSize_ = nApplProcs_;
    proctab_ = (MPIR_PROCDESC_EXT *)malloc(nApplNodes_ * nTasks * sizeof(MPIR_PROCDESC_EXT));
    if (proctab_ == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to create proctable\n", strerror(errno));
        return STAT_ALLOCATE_ERROR;
    }
    for (i = 0; i < nApplNodes_; i++)
    {
        snprintf(name, BUFSIZE, "a%d", i);
        for (j = 0; j < nTasks; j++)
        {
            proctab_[i * nTasks + j].mpirank = i * nTasks + j;
            proctab_[i * nTasks + j].pd.host_name = strdup(name);
            proctab_[i * nTasks + j].pd.executable_name = NULL;
        }
    }
    endTime.setTime();
    addPerfData("\tProctable Creation Time", (endTime - startTime).getDoubleTime());

    /* Generate the daemon rank map */
    startTime.setTime();
    statError = createDaemonRankMap();
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to create daemon rank map\n");
        return statError;
    }
    endTime.setTime();
    addPerfData("\tCreate Daemon Rank Map Time", (endTime - startTime).getDoubleTime());

    /* Update the application node list and daemon ranks lists */
    startTime.setTime();
    statError = setRanksList();
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to set ranks list\n");
        return statError;
    }
    endTime.setTime();
    addPerfData("\tRanks List Creation Time", (endTime - startTime).getDoubleTime());
    
    totalEnd.setTime();
    addPerfData("\tTotal STATBench Setup Time", (totalEnd - totalStart).getDoubleTime());

    startTime.setTime();
        
    if (isConnected_ == false)
    {
        printMsg(STAT_NOT_CONNECTED_ERROR, __FILE__, __LINE__, "STAT daemons have not been launched\n");
        return STAT_NOT_CONNECTED_ERROR;
    }

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "Generating traces with parameters %d %d %d %d %d...\n", maxDepth, nTasks, nTraces, functionFanout, nEqClasses);

    /* Send request to daemons to gather stack traces and wait for confirmation */
    if (broadcastStream_->send(PROT_STATBENCH_CREATE_TRACES, "%ud %ud %ud %ud %d", maxDepth, nTasks, nTraces, functionFanout, nEqClasses) == -1) 
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to send request to create traces\n");
        return STAT_MRNET_ERROR;
    }
    if (broadcastStream_->flush() != 0)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to flush message\n");
        return STAT_MRNET_ERROR;
    }
   
    /* Receive a single acknowledgement packet that all daemons have completed */
    do
    {
        retval = broadcastStream_->recv(&tag, packet, false);
        if (retval == 0)
        {
            if (WIFKILLED(lmonState))
            {
                printMsg(STAT_APPLICATION_EXITED, __FILE__, __LINE__, "LMON detected the application has exited\n");
                return STAT_APPLICATION_EXITED;
            }
            if (!WIFBESPAWNED(lmonState))
            {
                printMsg(STAT_DAEMON_ERROR, __FILE__, __LINE__, "LMON detected the daemons have exited\n");
                return STAT_DAEMON_ERROR;
            }
            usleep(1000);
        }
    }
    while (retval == 0);
    if (retval == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to receive ack\n");
        return STAT_MRNET_ERROR;
    }
    if (tag != PROT_STATBENCH_CREATE_TRACES_RESP)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Unexpected tag %d\n, expecting PROT_STATBENCH_CREATE_TRACES_RESP = %d\n", tag, PROT_STATBENCH_CREATE_TRACES_RESP);
    }

    /* Unpack the ack and check for errors */
    if (packet->unpack("%d", &retval) == -1)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to unpack acknowledgement packet\n");
        return STAT_MRNET_ERROR;
    }
    if (retval != 0)
    {
        printMsg(STAT_SAMPLE_ERROR, __FILE__, __LINE__, "%d daemons reported an error\n", retval);
        return STAT_SAMPLE_ERROR;
    }

    printMsg(STAT_STDOUT, __FILE__, __LINE__, "\t\tTraces generated!\n");
        
    endTime.setTime();
    addPerfData("Create Traces Time", (endTime - startTime).getDoubleTime());
    return STAT_OK;
}

