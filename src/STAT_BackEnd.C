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

#include "STAT_BackEnd.h"

using namespace std;
using namespace MRN;
using namespace Dyninst;
using namespace Dyninst::Stackwalker;
using namespace Dyninst::SymtabAPI;
#ifdef SW_VERSION_8_0_0
    using namespace Dyninst::ProcControlAPI;
#endif
#ifdef STAT_FGFS
    using namespace FastGlobalFileStatus;
    using namespace FastGlobalFileStatus::MountPointAttribute;
    using namespace FastGlobalFileStatus::CommLayer;
#endif

/* Externals from STAT's graphlib routines */
extern graphlib_functiontable_p statBitVectorFunctions;
extern graphlib_functiontable_p statCountRepFunctions;

FILE *statOutFp = NULL;

//TODO:
bool isPyTrace;
bool isFirstPythonFrame;

StatError_t statInit(int *argc, char ***argv, StatDaemonLaunch_t launchType)
{
    lmon_rc_e lmonRet;

    if (launchType == STATD_LMON_LAUNCH)
    {
        /* Call LaunchMON's Init function */
        lmonRet = LMON_be_init(LMON_VERSION, argc, argv);
        if (lmonRet != LMON_OK)
        {
            fprintf(stderr, "Failed to initialize Launchmon\n");
            return STAT_LMON_ERROR;
        }
    }

    return STAT_OK;
}

StatError_t statFinalize(StatDaemonLaunch_t launchType)
{
    lmon_rc_e lmonRet;

    if (launchType == STATD_LMON_LAUNCH)
    {
        /* Call LaunchMON's Finalize function */
        lmonRet = LMON_be_finalize();
        if (lmonRet != LMON_OK)
        {
            fprintf(stderr, "Failed to finalize Launchmon\n");
            return STAT_LMON_ERROR;
        }
    }

    return STAT_OK;
}

STAT_BackEnd::STAT_BackEnd(StatDaemonLaunch_t launchType) : launchType_(launchType)
{
    int intRet;
    char abuf[INET_ADDRSTRLEN], fileName[BUFSIZE], *envValue;
    FILE *file;
    struct sockaddr_in addr, *sinp = NULL;
    struct addrinfo *addinf = NULL;
    graphlib_error_t graphlibError;

    /* get hostname and IP address */
    intRet = gethostname(localHostName_, BUFSIZE);
    if (intRet != 0)
        fprintf(stderr, "Warning: gethostname returned %d\n", intRet);
    intRet = getaddrinfo(localHostName_, NULL, NULL, &addinf);
    if (intRet != 0)
        fprintf(stderr, "Warning: getaddrinfo returned %d\n", intRet);
    if (addinf != NULL)
        sinp = (struct sockaddr_in *)addinf->ai_addr;
    if (sinp != NULL)
        snprintf(localIp_, BUFSIZE, "%s", inet_ntop(AF_INET, &sinp->sin_addr, abuf, INET_ADDRSTRLEN));
    if (addinf != NULL)
        freeaddrinfo(addinf);

#ifdef BGL
    errOutFp_ = stdout;
#else
    errOutFp_ = stderr;
#endif

    graphlibError = graphlib_Init();
    if (GRL_IS_FATALERROR(graphlibError))
        fprintf(stderr, "Failed to initialize Graphlib\n");
    statInitializeReorderFunctions();
    statInitializeBitVectorFunctions();
    statInitializeMergeFunctions();
    statInitializeCountRepFunctions();

    /* Enable MRNet logging */
    envValue = getenv("STAT_MRNET_OUTPUT_LEVEL");
    if (envValue != NULL)
        set_OutputLevel(atoi(envValue));
    envValue = getenv("STAT_MRNET_DEBUG_LOG_DIRECTORY");
    if (envValue != NULL)
        setenv("MRNET_DEBUG_LOG_DIRECTORY", envValue, 1);

    /* Code to redirect output to a file */
    envValue = getenv("STAT_OUTPUT_REDIRECT_DIR");
    if (envValue != NULL)
    {
        snprintf(fileName, BUFSIZE, "%s/%s.stdout.txt", envValue, localHostName_);
        freopen(fileName, "w", stdout);
        snprintf(fileName, BUFSIZE, "%s/%s.stderr.txt", envValue, localHostName_);
        freopen(fileName, "w", stderr);
    }

    envValue = getenv("STAT_NO_GROUP_OPS");
    doGroupOps_ = (envValue == NULL);

    /* Code to setup StackWalker debug logging */
    envValue = getenv("STAT_SW_DEBUG_LOG_DIR");
    if (envValue != NULL)
    {
        snprintf(fileName, BUFSIZE, "%s/%s.sw.txt", envValue, localHostName_);
        file = fopen(fileName, "w");
        Dyninst::Stackwalker::setDebug(true);
        Dyninst::Stackwalker::setDebugChannel(file);
#ifdef SW_VERSION_8_0_0
        Dyninst::ProcControlAPI::setDebug(true);
        Dyninst::ProcControlAPI::setDebugChannel(file);
#endif
    }

    processMapNonNull_ = 0;
    statOutFp = NULL;
    initialized_ = false;
    connected_ = false;
    isRunning_ = false;
    network_ = NULL;
    proctab_ = NULL;
    proctabSize_ = 0;
    parentHostName_ = NULL;
    prefixTree3d_ = NULL;
    prefixTree2d_ = NULL;
    broadcastStream_ = NULL;
#ifdef STAT_FGFS
    fgfsCommFabric_ = NULL;
#endif
}

STAT_BackEnd::~STAT_BackEnd()
{
    int i;
    graphlib_error_t graphlibError;
    map<int, StatBitVectorEdge_t *>::iterator nodeInEdgeMapIter;

    /* delete exisitng graphs */
    if (prefixTree3d_ != NULL)
    {
        graphlibError = graphlib_delGraph(prefixTree3d_);
        if (GRL_IS_FATALERROR(graphlibError))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
        prefixTree3d_ = NULL;
    }
    if (prefixTree2d_ != NULL)
    {
        graphlibError = graphlib_delGraph(prefixTree2d_);
        if (GRL_IS_FATALERROR(graphlibError))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
        prefixTree2d_ = NULL;
    }

    /* delete any stored edge labels */
    for (nodeInEdgeMapIter = nodeInEdgeMap_.begin(); nodeInEdgeMapIter != nodeInEdgeMap_.end(); nodeInEdgeMapIter++)
        statFreeEdge((void *)nodeInEdgeMapIter->second);
    nodeInEdgeMap_.clear();

    /* free the parent hostname */
    if (parentHostName_ != NULL)
    {
        free(parentHostName_);
        parentHostName_ = NULL;
    }

    /* free the process table */
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
        proctab_ = NULL;
    }
    statFreeReorderFunctions();
    statFreeBitVectorFunctions();
    statFreeCountRepFunctions();
    statFreeMergeFunctions();
    graphlibError = graphlib_Finish();
    if (GRL_IS_FATALERROR(graphlibError))
        fprintf(stderr, "Warning: Failed to finish graphlib\n");

#ifdef STAT_FGFS
    if (fgfsCommFabric_ != NULL)
    {
        delete fgfsCommFabric_;
        fgfsCommFabric_ = NULL;
    }
#endif
    /* clean up MRNet */
    if (network_ != NULL)
    {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "waiting for shutdown\n");
        network_->waitfor_ShutDown();
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "deleting network\n");
        delete network_;
        network_ = NULL;
    }
}

StatError_t STAT_BackEnd::init()
{
    int tempSize;
    lmon_rc_e lmonRet;

    /* Register unpack function with Launchmon */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Registering unpack function with Launchmon\n");
    lmonRet = LMON_be_regUnpackForFeToBe(unpackStatBeInfo);
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to register unpack function\n");
        return STAT_LMON_ERROR;
    }

    /* Launchmon handshake */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Launchmon handshaking\n");
    lmonRet = LMON_be_handshake(NULL);
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to handshake with Launchmon\n");
        return STAT_LMON_ERROR;
    }

    /* Wait for Launchmon to be ready*/
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Waiting for launchmon to be ready\n");
    lmonRet = LMON_be_ready(NULL);
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Launchmon failed to get ready\n");
        return STAT_LMON_ERROR;
    }

    /* Allocate my process table */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Allocating process table\n");
    lmonRet = LMON_be_getMyProctabSize(&tempSize);
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to get Process Table Size\n");
        return STAT_LMON_ERROR;
    }
    proctab_ = (MPIR_PROCDESC_EXT *)malloc(tempSize * sizeof(MPIR_PROCDESC_EXT));
    if (proctab_ == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Error allocating process table\n", strerror(errno));
        return STAT_ALLOCATE_ERROR;
    }

    /* Get my process table entries */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Getting my process table entries\n");
    lmonRet = LMON_be_getMyProctab(proctab_, &proctabSize_, tempSize);
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to get Process Table\n");
        return STAT_LMON_ERROR;
    }

    initialized_ = true;
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Launchmon successfully initialized\n");

    return STAT_OK;
}


StatError_t STAT_BackEnd::addSerialProcess(const char *pidString)
{
    static int rank = -1;
    unsigned int pid;
    char *remoteNode = NULL, *executablePath = NULL;
    string::size_type delimPos;
    string remotePid = pidString, remoteHost;
        
    rank++;
    
    delimPos = remotePid.find_first_of("@");
    if (delimPos != string::npos)
    {
        executablePath = strdup(remotePid.substr(0, delimPos).c_str());
        if (executablePath == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed on call to strdup(%s) to executablePath\n", strerror(errno), remotePid.substr(0, delimPos).c_str());
            return STAT_ALLOCATE_ERROR;
        }
        remotePid = remotePid.substr(delimPos + 1, remotePid.length());
    }
    else
    {
        executablePath = strdup("output");
        if (executablePath == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed on call to strdup to executablePath\n", strerror(errno));
            return STAT_ALLOCATE_ERROR;
        }
    }

    delimPos = remotePid.find_first_of(":");
    if (delimPos != string::npos)
    {
        remoteHost = remotePid.substr(0, delimPos);
        remoteNode = strdup(remoteHost.c_str());
        if (remoteNode == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed to strdup(%s) to remoteNode\n", strerror(errno), remotePid.c_str());
            return STAT_ALLOCATE_ERROR;
        }
        pid = atoi((remotePid.substr(delimPos + 1, remotePid.length())).c_str());
    }
    else
        pid = atoi(remotePid.c_str());
        
    if (strcmp(remoteHost.c_str(), "localhost") == 0 || strcmp(remoteHost.c_str(), localHostName_) == 0)
    {
        proctabSize_++;
        proctab_ = (MPIR_PROCDESC_EXT *)realloc(proctab_, proctabSize_ * sizeof(MPIR_PROCDESC_EXT));
        if (proctab_ == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to allocate memory for the process table\n", strerror(errno));
            return STAT_ALLOCATE_ERROR;
        }

        proctab_[proctabSize_ - 1].mpirank = rank;
        proctab_[proctabSize_ - 1].cnodeid = rank;
        if (remoteNode != NULL)
            proctab_[proctabSize_ - 1].pd.host_name = remoteNode;
        else
        {
            proctab_[proctabSize_ - 1].pd.host_name = strdup("localhost");
            if (proctab_[proctabSize_ - 1].pd.host_name == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed on call to strdup to proctab_[%d]\n", strerror(errno), proctabSize_ - 1);
                return STAT_ALLOCATE_ERROR;
            }
        }
        proctab_[proctabSize_ - 1].pd.executable_name = executablePath;
        proctab_[proctabSize_ - 1].pd.pid = pid;
    }

    initialized_ = true;
    return STAT_OK;
}

StatError_t STAT_BackEnd::connect(int argc, char **argv)
{
    int i;
    bool found;
    char *param[6], parentPort[BUFSIZE], parentRank[BUFSIZE], myRank[BUFSIZE];
    string prettyHost, leafPrettyHost;
    lmon_rc_e lmonRet;
    StatLeafInfoArray_t leafInfoArray;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Prepearing to connect to MRNet\n");

    if (argc == 0 || argv == NULL)
    {
        /* Make sure we've been initialized through Launchmon */
        if (initialized_ == false)
        {
            printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Trying to connect when not initialized\n");
            return STAT_LMON_ERROR;
        }
    
        /* Receive MRNet connection information */
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Receiving connection information from FE\n");
        lmonRet = LMON_be_recvUsrData((void *)&leafInfoArray);
        if (lmonRet != LMON_OK)
        {
            printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to receive data from FE\n");
            return STAT_LMON_ERROR;
        }
    
        /* Master broadcast number of daemons */
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Broadcasting size of connection info to daemons\n");
        lmonRet = LMON_be_broadcast((void *)&(leafInfoArray.size), sizeof(int));
        if (lmonRet != LMON_OK)
        {
            printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to broadcast num_leaves\n");
            return STAT_LMON_ERROR;
        }
    
        /* Non-masters allocate space for the MRNet connection info */
        if (LMON_be_amIMaster() == LMON_NO)
        {
            leafInfoArray.leaves = (StatLeafInfo_t *)malloc(leafInfoArray.size * sizeof(StatLeafInfo_t));
            if (leafInfoArray.leaves == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to allocate %d x %d = %dB of memory for leaf info\n", leafInfoArray.size, sizeof(StatLeafInfo_t), leafInfoArray.size*sizeof(StatLeafInfo_t));
                return STAT_ALLOCATE_ERROR;
            }
        }
    
        /* Master broadcast MRNet connection information to all daemons */
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Broadcasting connection information to all daemons\n");
        lmonRet = LMON_be_broadcast((void *)(leafInfoArray.leaves), leafInfoArray.size * sizeof(StatLeafInfo_t));
        if (lmonRet != LMON_OK)
        {
            printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to broadcast num_leaves\n");
            return STAT_LMON_ERROR;
        }
    
        /* Find my MRNet personality  */
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Searching for my connection information\n");
        found = false;
        XPlat::NetUtils::GetHostName(localHostName_, prettyHost);
        for (i = 0; i < leafInfoArray.size; i++)
        {
            XPlat::NetUtils::GetHostName(string(leafInfoArray.leaves[i].hostName), leafPrettyHost);
            if (prettyHost == leafPrettyHost || strcmp(leafPrettyHost.c_str(), localIp_) == 0)
            {
                found = true;
                parentHostName_ = strdup(leafInfoArray.leaves[i].parentHostName);
                if (parentHostName_ == NULL)
                {
                    printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed on call to strdup(%s) to parentHostName_)\n", strerror(errno), leafInfoArray.leaves[i].parentHostName);
                    return STAT_ALLOCATE_ERROR;
                }
                parentPort_ = leafInfoArray.leaves[i].parentPort;
                parentRank_ = leafInfoArray.leaves[i].parentRank;
                myRank_ = leafInfoArray.leaves[i].rank;
                break;
            }
        }
        if (found == false)
        {
            printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to find MRNet parent info\n");
            return STAT_MRNET_ERROR;
        }
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Found MRNet connection info, parent hostname = %s, parent port = %d, my MRNet rank = %d\n", parentHostName_, parentPort_, myRank_);
    
        /* Connect to the MRNet Network */
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Connecting to MRNet network\n");
        snprintf(parentPort, BUFSIZE, "%d", parentPort_);
        snprintf(parentRank, BUFSIZE, "%d", parentRank_);
        snprintf(myRank, BUFSIZE, "%d", myRank_);
        param[0] = NULL;
        param[1] = parentHostName_;
        param[2] = parentPort;
        param[3] = parentRank;
        param[4] = localHostName_;
        param[5] = myRank;
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Calling CreateNetworkBE with 6 args:\n");
        for (i = 0; i < 6; i++)
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "\targv[%d] = %s\n", i, param[i]);
        network_ = Network::CreateNetworkBE(6, param);
        if (network_ == NULL)
        {
            printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "CreateNetworkBE() failed\n");
            return STAT_MRNET_ERROR;
        }
        printMsg(STAT_LOG_MESSAGE,__FILE__,__LINE__,"CreateNetworkBE successfully completed\n");
    
        /* Free the Leaf Info array */
        if (leafInfoArray.leaves != NULL)
            free(leafInfoArray.leaves);
    } // if (argc == 0 || argv == NULL)
    else
    {
        /* We were launched by MRNet */
        /* Store my connection information */
        parentHostName_ = strdup(argv[1]);
        if (parentHostName_ == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed on call to strdup(%s) to parentHostName_\n", strerror(errno), argv[1]);
            return STAT_ALLOCATE_ERROR;
        }
        parentPort_ = atoi(argv[2]);
        parentRank_ = atoi(argv[3]);
        myRank_ = atoi(argv[5]);

        /* Connect to the MRNet Network */
        printMsg(STAT_LOG_MESSAGE, __FILE__ ,__LINE__, "Calling CreateNetworkBE with %d args:\n", argc);
        for (i = 0; i < argc; i++)
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "\targv[%d] = %s\n", i, argv[i]);
        network_ = Network::CreateNetworkBE(6, argv);
        if (network_ == NULL)
        {
            printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "CreateNetworkBE() failed\n");
            return STAT_MRNET_ERROR;
        }
        printMsg(STAT_LOG_MESSAGE,__FILE__,__LINE__,"CreateNetworkBE successfully completed\n");
    } // else (argc == 0 || argv == NULL)

    connected_ = true;
    return STAT_OK;
}

StatError_t STAT_BackEnd::mainLoop()
{
    int tag = 0, intRet, nEqClasses, swNotificationFd, mrnNotificationFd, maxFd, iCountRep, nodeId, bitVectorLength, stopArrayLen;
    unsigned int nTraces, traceFrequency, nTasks, functionFanout, maxDepth, nRetries, retryFrequency, withThreads, clearOnSample, withPython, *stopArray = NULL;
    uint64_t obyteArrayLen;
    char *obyteArray = NULL, *variableSpecification = NULL;
    bool boolRet, shouldBlock;
    StatSample_t previousSampleType;
    Stream *stream = NULL;
    fd_set readFds, writeFds, exceptFds;
    PacketPtr packet;
    StatError_t statError;
    graphlib_error_t graphlibError;
    StatBitVectorEdge_t *edge;
    map<int, Walker *>::iterator processMapIter;
    ProcDebug *pDebug;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Beginning to execute main loop\n");

    if (connected_ == false)
    {
        printMsg(STAT_NOT_CONNECTED_ERROR, __FILE__, __LINE__, "Not connected to MRNet\n");
        return STAT_NOT_CONNECTED_ERROR;
    }

    do
    {
        /* Set the stackwalker notification file descriptor */
        if (processMap_.size() > 0 and processMapNonNull_ > 0)
            swNotificationFd = ProcDebug::getNotificationFD();
        else
            swNotificationFd = -1;

        if (swNotificationFd != -1)
        {
            /* set the MRNet notification FD if StackWalker FD set, otherwise we can just use blocking receives */
            mrnNotificationFd = network_->get_EventNotificationFd(MRN::Event::DATA_EVENT);

            /* setup the select call for the MRNet and SW FDs */
            if (mrnNotificationFd > swNotificationFd)
                maxFd = mrnNotificationFd + 1;
            else
                maxFd = swNotificationFd + 1;
            FD_ZERO(&readFds);
            FD_ZERO(&writeFds);
            FD_ZERO(&exceptFds);
            FD_SET(mrnNotificationFd, &readFds);
            FD_SET(swNotificationFd, &readFds);

            /* call select to get pending requests */
            intRet = select(maxFd, &readFds, &writeFds, &exceptFds, NULL);
            if (intRet < 0 && errno != EINTR)
            {
                printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "%s: select failed\n", strerror(errno));
                return STAT_SYSTEM_ERROR;
            }
            if (FD_ISSET(mrnNotificationFd, &readFds))
            {
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Handling MRNet event on FD %d\n", mrnNotificationFd);
                network_->clear_EventNotificationFd(MRN::Event::DATA_EVENT);
            }
            else if (FD_ISSET(swNotificationFd, &readFds))
            {
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Handling debug event on FD %d\n", swNotificationFd);
                boolRet = ProcDebug::handleDebugEvent(false);
                if (boolRet == false)
                {
                   printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Error handling debug events: %s\n", Stackwalker::getLastErrorMsg());
                    return STAT_STACKWALKER_ERROR;
                }

                /* Check is target processes exited, if so, set Walker to NULL */
                for (processMapIter = processMap_.begin(); processMapIter != processMap_.end(); processMapIter++)
                {
                    if (processMapIter->second == NULL)
                        continue;
                    pDebug = dynamic_cast<ProcDebug *>(processMapIter->second->getProcessState());
                    if (pDebug != NULL)
                    {
                        if (pDebug->isTerminated() == true)
                        {
                            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Task %d has exited\n", processMapIter->first);
                            delete processMapIter->second;
                            processMapNonNull_ = processMapNonNull_ - 1;
                            processMapIter->second = NULL;
                            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Process map now contains %d processes, non-null count = %d\n", processMap_.size(), processMapNonNull_);
                        }
                    }
                }
                continue;
            }
        } /* if (swNotificationFd != -1) */

        /* Receive the packet from the STAT FE */
        shouldBlock = (swNotificationFd == -1);
        intRet = network_->recv(&tag, packet, &stream, shouldBlock);
        if (intRet == 0)
            continue;
        else if (intRet != 1)
        {
            printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::recv() failure\n");
            return STAT_MRNET_ERROR;
        }
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Packet received with tag %d\n", tag);

        /* Initialize return value to error code */
        intRet = 1;

        switch(tag)
        {
            case PROT_ATTACH_APPLICATION:
                /* Attach to the application */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to attach\n");
                statError = attach();
                if (statError == STAT_OK)
                    intRet = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Attach complete, sending ackowledgement\n");
                if (sendAck(stream, PROT_ATTACH_APPLICATION_RESP, intRet) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_ATTACH_APPLICATION_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }
                break;

            case PROT_PAUSE_APPLICATION:
                /* Pause to the application */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to pause\n");
                statError = pause();
                if (statError == STAT_OK)
                    intRet = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Pause complete, sending ackowledgement\n");
                if (sendAck(stream, PROT_PAUSE_APPLICATION_RESP, intRet) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_PAUSE_APPLICATION_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }
                break;

            case PROT_RESUME_APPLICATION:
                /* Resume to the application */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to resume\n");
                statError = resume();
                if (statError == STAT_OK)
                    intRet = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Resume complete, sending ackowledgement\n");
                if (sendAck(stream, PROT_RESUME_APPLICATION_RESP, intRet) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_RESUME_APPLICATION_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }
                break;

            case PROT_SAMPLE_TRACES:
                /* Unpack the message */
                previousSampleType = sampleType_;
                if (packet->unpack("%ud %ud %ud %ud %ud %ud %ud %ud %s", &nTraces, &traceFrequency, &nRetries, &retryFrequency, &sampleType_, &withThreads, &clearOnSample, &withPython, &variableSpecification) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_SAMPLE_TRACES) failed\n");
                    if (sendAck(stream, PROT_SAMPLE_TRACES_RESP, intRet) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_SAMPLE_TRACES_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }
                    return STAT_MRNET_ERROR;
                }
                if (previousSampleType != sampleType_)
                    clearOnSample = 1;
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to sample %d traces each %d us with %d retries every %d us with variables %s\n", nTraces, traceFrequency, nRetries, retryFrequency, variableSpecification);

                /* Gather stack traces */
                withPython_ = false;
                if (withPython != 0)
                    withPython_ = true;

                statError = sampleStackTraces(nTraces, traceFrequency, nRetries, retryFrequency, withThreads, clearOnSample, variableSpecification, withPython);
                if (statError == STAT_OK)
                    intRet = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Sampling complete, sending ackowledgement\n");
                if (sendAck(stream, PROT_SAMPLE_TRACES_RESP, intRet) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_SAMPLE_TRACES_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }

                break;

            case PROT_SEND_LAST_TRACE:
                /* Send merged traces to the FE */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to send traces to the FE\n");

                /* Free previously allocated byte array */
                if (obyteArray != NULL)
                {
                    free(obyteArray);
                    obyteArray = NULL;
                }

                /* Serialize 2D graph */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Serializing 2D graph\n");
                graphlibError = graphlib_serializeBasicGraph(prefixTree2d_, &obyteArray, &obyteArrayLen);
                if (GRL_IS_FATALERROR(graphlibError))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to serialize 2D prefix tree\n");
                    return STAT_GRAPHLIB_ERROR;
                }

                /* Send */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Sending graphs to FE\n");
                bitVectorLength = statBitVectorLength(proctabSize_);
#ifdef MRNET40
                if (stream->send(PROT_SEND_LAST_TRACE_RESP, "%Ac %d %d %ud", obyteArray, obyteArrayLen, bitVectorLength, myRank_, sampleType_) == -1)
#else
                if (stream->send(PROT_SEND_LAST_TRACE_RESP, "%ac %d %d %ud", obyteArray, obyteArrayLen, bitVectorLength, myRank_, sampleType_) == -1)
#endif
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::send() failure\n");
                    return STAT_MRNET_ERROR;
                }
                if (stream->flush() == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::flush() failure\n");
                    return STAT_MRNET_ERROR;
                }

                break;

            case PROT_SEND_TRACES:
                /* Send merged traces to the FE */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to send traces to the FE\n");

                /* Free previously allocated byte array */
                if (obyteArray != NULL)
                {
                    free(obyteArray);
                    obyteArray = NULL;
                }

                /* Serialize 3D graph */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Serializing 3D graph\n");
                graphlibError = graphlib_serializeBasicGraph(prefixTree3d_, &obyteArray, &obyteArrayLen);
                if (GRL_IS_FATALERROR(graphlibError))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to serialize 3D prefix tree\n");
                    return STAT_GRAPHLIB_ERROR;
                }

                /* Send */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Sending graphs to FE\n");
                bitVectorLength = statBitVectorLength(proctabSize_);
#ifdef MRNET40
                if (stream->send(PROT_SEND_TRACES_RESP, "%Ac %d %d %ud", obyteArray, obyteArrayLen, bitVectorLength, myRank_, sampleType_) == -1)
#else
                if (stream->send(PROT_SEND_TRACES_RESP, "%ac %d %d %ud", obyteArray, obyteArrayLen, bitVectorLength, myRank_, sampleType_) == -1)
#endif
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::send() failure\n");
                    return STAT_MRNET_ERROR;
                }
                if (stream->flush() == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::flush() failure\n");
                    return STAT_MRNET_ERROR;
                }

                break;

            case PROT_SEND_NODE_IN_EDGE:
                /* Send requested edge label to the FE */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to send edge label to the FE\n");

                /* Unpack the message */
                previousSampleType = sampleType_;
                if (packet->unpack("%d", &nodeId) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_SEND_NODE_IN_EDGE) failed\n");
                    return STAT_MRNET_ERROR;
                }

                /* Free previously allocated byte array */
                if (obyteArray != NULL)
                {
                    free(obyteArray);
                    obyteArray = NULL;
                }

                /* Find the requested edge label or create an empty dummy one */
                if (nodeInEdgeMap_.find(nodeId) != nodeInEdgeMap_.end())
                    edge = nodeInEdgeMap_[nodeId];
                else
                {
                    edge = initializeBitVectorEdge(proctabSize_);
                    if (edge == NULL)
                    {
                        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to initialize edge\n");
                        return STAT_ALLOCATE_ERROR;
                    }
                }

                /* Serialize the edge label */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Serializing the edge label\n");
                obyteArrayLen = statSerializeEdgeLength(edge);
                obyteArray = (char *)malloc(obyteArrayLen);
                statSerializeEdge(obyteArray, edge);

                /* Free the dummy edge label */
                if (nodeInEdgeMap_.find(nodeId) == nodeInEdgeMap_.end())
                    statFreeEdge(edge);

                /* Send */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Sending edge to FE\n");
                bitVectorLength = statBitVectorLength(proctabSize_);
#ifdef MRNET40
                if (stream->send(PROT_SEND_NODE_IN_EDGE_RESP, "%Ac %d %d %ud", obyteArray, obyteArrayLen, bitVectorLength, myRank_, sampleType_) == -1)
#else
                if (stream->send(PROT_SEND_NODE_IN_EDGE_RESP, "%ac %d %d %ud", obyteArray, obyteArrayLen, bitVectorLength, myRank_, sampleType_) == -1)
#endif
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::send() failure\n");
                    return STAT_MRNET_ERROR;
                }
                if (stream->flush() == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::flush() failure\n");
                    return STAT_MRNET_ERROR;
                }

                break;

            case PROT_DETACH_APPLICATION:
                /* Detach from the application */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to detach\n");

                /* Unpack message */
                if (packet->unpack("%aud", &stopArray, &stopArrayLen) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_DETACH_APPLICATION) failed\n");
                    if (sendAck(stream, PROT_DETACH_APPLICATION_RESP, intRet) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_DETACH_APPLICATION_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }
                    return STAT_MRNET_ERROR;
                }

                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Detaching, leaving %d processes stopped\n", stopArrayLen);
                statError = detach(stopArray, stopArrayLen);
                if (statError ==  STAT_OK)
                    intRet = 0;

                if (stopArray != NULL)
                    free(stopArray);

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Detaching complete, sending ackowledgement\n");
                if (sendAck(stream, PROT_DETACH_APPLICATION_RESP, intRet) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_DETACH_APPLICATION_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }
                break;

            case PROT_TERMINATE_APPLICATION:
                /* Terminate the application */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to terminate application\n");
                statError = terminateApplication();
                if (statError ==  STAT_OK)
                    intRet = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Termination complete, sending ackowledgement\n");
                if (sendAck(stream, PROT_TERMINATE_APPLICATION_RESP, intRet) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_TERMINATE_APPLICATION_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }

                break;

            case PROT_STATBENCH_CREATE_TRACES:
                /* Create stack traces */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to create traces\n");

                /* Unpack message */
                if (packet->unpack("%ud %ud %ud %ud %d %d", &maxDepth, &nTasks, &nTraces, &functionFanout, &nEqClasses, &iCountRep) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_STATBENCH_CREATE_TRACES) failed\n");
                    if (sendAck(stream, PROT_STATBENCH_CREATE_TRACES_RESP, intRet) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_STATBENCH_CREATE_TRACES_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }
                    return STAT_MRNET_ERROR;
                }
                if (iCountRep == 1)
                    sampleType_ = STAT_CR_FUNCTION_NAME_ONLY;
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to create traces with max depth = %d, num tasks = %d, num traces = %d, function fanout = %d, equivalence classes = %d, count rep = %d\n", maxDepth, nTasks, nTraces, functionFanout, nEqClasses, iCountRep);

                /* Create stack traces */
                statError = statBenchCreateTraces(maxDepth, nTasks, nTraces, functionFanout, nEqClasses);
                if (statError == STAT_OK)
                    intRet = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Finished creating traces, sending ackowledgement\n");
                if (sendAck(stream, PROT_STATBENCH_CREATE_TRACES_RESP, intRet) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_STATBENCH_CREATE_TRACES_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }

                break;

            case PROT_CHECK_VERSION:
                /* Check version */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to check version\n");

                /* Unpack message */
                int major, minor, revision;
                if (packet->unpack("%d %d %d", &major, &minor, &revision) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_CHECK_VERSION) failed\n");
                    if (sendAck(stream, PROT_CHECK_VERSION_RESP, intRet) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_CHECK_VERSION_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }
                    return STAT_MRNET_ERROR;
                }
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to compare my version %d.%d.%d to the FE version %d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION, major, minor, revision);

                /* Compare version numbers */
                if (major == STAT_MAJOR_VERSION && minor == STAT_MINOR_VERSION && revision == STAT_REVISION_VERSION)
                    intRet = 0;

                /* Send ack packet */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Finished comparing version, sending ackowledgement\n");
                if (stream->send(PROT_CHECK_VERSION_RESP, "%d %d %d %d %d", major, minor, revision, intRet, 0) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::send() failure\n");
                    return STAT_MRNET_ERROR;
                }
                if (stream->flush() == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::flush() failure\n");
                    return STAT_MRNET_ERROR;
                }

                if (intRet != 0)
                    printMsg(STAT_VERSION_ERROR, __FILE__, __LINE__, "Daemon reports version mismatch: FE = %d.%d.%d, Daemon = %d.%d.%d\n", major, minor, revision, STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);

                break;

            case PROT_SEND_BROADCAST_STREAM:
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received broadcast stream\n");
                broadcastStream_ = stream;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Finished creating traces, sending ackowledgement\n");
                if (sendAck(stream, PROT_SEND_BROADCAST_STREAM_RESP, 0) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_STATBENCH_CREATE_TRACES_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }
                break;

#ifdef STAT_FGFS
            case PROT_SEND_FGFS_STREAM:
                /* Initialize the FGFS Comm Fabric */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received message to setup FGFS\n");
                boolRet = MRNetCommFabric::initialize((void *)network_, (void *)stream);
                if (boolRet == false)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to initialize FGFS CommFabric\n");
                    return STAT_MRNET_ERROR;
                }
                fgfsCommFabric_ = new MRNetCommFabric();
                boolRet = AsyncGlobalFileStatus::initialize(fgfsCommFabric_);
                if (boolRet == false)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to initialize AsyncGlobalFileStatus\n");
                    return STAT_MRNET_ERROR;
                }
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "FGFS setup complete\n");

                break;

            case PROT_FILE_REQ:
                MRNetSymbolReaderFactory *msrf;
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to register file request stream \n");

                /* Save the stream for sending file requests later */
                msrf = new MRNetSymbolReaderFactory(Walker::getSymbolReader(), network_, stream);
                Walker::setSymbolReader(msrf);
                intRet = 0;

                /* Send file request acknowledgement */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Finished registering file request stream, sending ackowledgement\n");
                if (sendAck(broadcastStream_, PROT_FILE_REQ_RESP, intRet) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_FILE_REQ_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }

                break;
#endif //FGFS

            case PROT_EXIT:
                /* Exit */
                break;

            default:
                /* Unknown tag */
                printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Unknown Tag %d, first = %d, last = %d\n", tag, PROT_ATTACH_APPLICATION, PROT_SEND_NODE_IN_EDGE_RESP);
                break;
        } /* switch */
    } while (tag != PROT_EXIT);

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received message to exit\n");

    /* Free allocated byte arrays */
    if (obyteArray != NULL)
    {
        free(obyteArray);
        obyteArray = NULL;
    }

    return STAT_OK;
}

StatError_t STAT_BackEnd::attach()
{
    int i;
    Walker *proc;
    map<int, Walker *>::iterator processMapIter;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Attaching to all application processes\n");

#if defined(GROUP_OPS)
    ThreadTracking::setDefaultTrackThreads(false);
    LWPTracking::setDefaultTrackLWPs(false);
    vector<ProcessSet::AttachInfo> aInfo;
    if (doGroupOps_)
    {
        aInfo.reserve(proctabSize_);
        for (i = 0; i < proctabSize_; i++)
        {
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Group attach includes process %s, pid %d, MPI rank %d\n", proctab_[i].pd.executable_name, proctab_[i].pd.pid, proctab_[i].mpirank);
            ProcessSet::AttachInfo pAttach;
            pAttach.pid = proctab_[i].pd.pid;
            pAttach.executable = proctab_[i].pd.executable_name;
            pAttach.error_ret = ProcControlAPI::err_none;
            aInfo.push_back(pAttach);
        }
        procSet_ = ProcessSet::attachProcessSet(aInfo);
        walkerSet_ = WalkerSet::newWalkerSet();
    }
#endif

    /* Attach to the processes in the process table */
    for (i = 0; i < proctabSize_; i++)
    {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Attaching to process %s, pid %d, MPI rank %d\n", proctab_[i].pd.executable_name, proctab_[i].pd.pid, proctab_[i].mpirank);

        Walker *proc;
#if defined(GROUP_OPS)
        if (doGroupOps_)
        {
            Process::ptr pcProc = aInfo[i].proc;
            proc = pcProc ? Walker::newWalker(pcProc) : NULL;
            if (proc == NULL)
            {
                printMsg(STAT_WARNING, __FILE__, __LINE__, "Group Attach to task rank %d, pid %d failed with message '%s'\n", proctab_[i].mpirank, proctab_[i].pd.pid, Dyninst::ProcControlAPI::getGenericErrorMsg(aInfo[i].error_ret));
            }
            else 
            {
                pcProc->setData(proc); /* Up ptr for mapping Process::ptr -> Walker */
                walkerSet_->insert(proc);
            }
        }
        else
#endif
        {
            proc = Walker::newWalker(proctab_[i].pd.pid, proctab_[i].pd.executable_name);
            if (proc == NULL)
            {
                printMsg(STAT_WARNING, __FILE__, __LINE__, "StackWalker Attach to task rank %d, pid %d failed with message '%s'\n", proctab_[i].mpirank, proctab_[i].pd.pid, Dyninst::Stackwalker::getLastErrorMsg());
            }
        }

        processMap_[proctab_[i].mpirank] = proc;
        if (proc != NULL) 
            processMapNonNull_++;
    }
    for (i = 0, processMapIter = processMap_.begin(); processMapIter != processMap_.end(); i++, processMapIter++)
        procsToRanks_.insert(make_pair(processMapIter->second, i));
    isRunning_ = false;

    return STAT_OK;
}

StatError_t STAT_BackEnd::pause()
{
    map<int, Walker *>::iterator processMapIter;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Pausing all application processes\n");

#if defined(GROUP_OPS)
    if (doGroupOps_)
        procSet_->stopProcs();
    else
#endif
    {
        for (processMapIter = processMap_.begin(); processMapIter != processMap_.end(); processMapIter++)
           pauseImpl(processMapIter->second);
    }
    isRunning_ = false;

    return STAT_OK;
}

StatError_t STAT_BackEnd::resume()
{
    map<int, Walker *>::iterator processMapIter;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Resuming all application processes\n");
#if defined(GROUP_OPS)
    if (doGroupOps_)
        procSet_->continueProcs();
    else
#endif
    {
        for (processMapIter = processMap_.begin(); processMapIter != processMap_.end(); processMapIter++)
            resumeImpl(processMapIter->second);
    }
    isRunning_ = true;

    return STAT_OK;
}

StatError_t STAT_BackEnd::pauseImpl(Walker *walker)
{
    /* Pause the process */
    bool boolRet;
    ProcDebug *pDebug;

    if (walker != NULL)
    {
        pDebug = dynamic_cast<ProcDebug *>(walker->getProcessState());
        if (pDebug == NULL)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to dynamic_cast ProcDebug pointer\n");
            return STAT_STACKWALKER_ERROR;
        }
        if (pDebug->isTerminated())
            return STAT_OK;
        boolRet = pDebug->pause();
        if (boolRet == false)
            printMsg(STAT_WARNING, __FILE__, __LINE__, "Failed to pause process\n");
    }
    return STAT_OK;
}

StatError_t STAT_BackEnd::resumeImpl(Walker *walker)
{
    /* Resume the process */
    bool boolRet;
    ProcDebug *pDebug;

    if (walker != NULL)
    {
        pDebug = dynamic_cast<ProcDebug *>(walker->getProcessState());
        if (pDebug == NULL)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to dynamic_cast ProcDebug pointer\n");
            return STAT_STACKWALKER_ERROR;
        }
        if (pDebug->isTerminated())
            return STAT_OK;
        boolRet = pDebug->resume();
        if (boolRet == false)
            printMsg(STAT_WARNING, __FILE__, __LINE__, "Failed to resume process\n");
    }
    return STAT_OK;
}

StatError_t STAT_BackEnd::parseVariableSpecification(char *variableSpecification)
{
    int nElements, i;
    string cur, spec, temp;
    string::size_type delimPos;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Parsing variable specification: %s\n", variableSpecification);

    /* First get the number of elements */
    spec = variableSpecification;
    delimPos = spec.find_first_of("#");
    temp = spec.substr(0, delimPos);
    nElements = atoi(temp.c_str());
    if (nElements < 0)
        nElements = 0;
    nVariables_ = nElements;
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "%d elements in variable specification\n", nElements);
    extractVariables_ = (StatVariable_t *)malloc(nElements * sizeof(StatVariable_t));
    if (extractVariables_ == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to allocate %d elements\n", nElements);
    }

    /* Get each element */
    cur = spec.substr(delimPos + 1, spec.length());
    for (i = 0; i < nElements; i++)
    {
        /* Get the file name */
        delimPos = cur.find_first_of(":");
        temp = cur.substr(0, delimPos);
        snprintf(extractVariables_[i].fileName, BUFSIZE, "%s", temp.c_str());
        cur = cur.substr(delimPos + 1, cur.length());

        /* Get the line number */
        delimPos = cur.find_first_of(".");
        temp = cur.substr(0, delimPos);
        extractVariables_[i].lineNum = atoi(temp.c_str());
        cur = cur.substr(delimPos + 1, cur.length());

        /* Get the stack depth */
        delimPos = cur.find_first_of("$");
        temp = cur.substr(0, delimPos);
        extractVariables_[i].depth = atoi(temp.c_str());
        cur = cur.substr(delimPos + 1, cur.length());

        /* Get the variable name */
        if (i == nElements - 1)
            delimPos = cur.length();
        else
            delimPos = cur.find_first_of(",");
        temp = cur.substr(0, delimPos);
        snprintf(extractVariables_[i].variableName, BUFSIZE, "%s", temp.c_str());
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "element: %s %d %d %s\n", extractVariables_[i].fileName, extractVariables_[i].lineNum, extractVariables_[i].depth, extractVariables_[i].variableName);
        if (i != nElements - 1)
            cur = cur.substr(delimPos + 1, cur.length());
    }

    return STAT_OK;
}

StatError_t STAT_BackEnd::getSourceLine(Walker *proc, Address addr, char *outBuf, int *lineNum)
{
    bool boolRet;
    Address loadAddr;
    LibraryState *libState;
    LibAddrPair lib;
    Symtab *symtab;
    vector<LineNoTuple *> lines;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Getting source line info for address %lx\n", addr);

    *lineNum = 0;
    if (proc == NULL)
    {
        printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Walker object == NULL\n");
        snprintf(outBuf, BUFSIZE, "?");
        return STAT_STACKWALKER_ERROR;
    }

    libState = proc->getProcessState()->getLibraryTracker();
    if (libState == NULL)
    {
        printMsg(STAT_WARNING, __FILE__, __LINE__, "Failed to get LibraryState\n");
        snprintf(outBuf, BUFSIZE, "?");
        return STAT_OK;
    }
    boolRet = libState->getLibraryAtAddr(addr, lib);
    if (boolRet == false)
    {
        printMsg(STAT_WARNING, __FILE__, __LINE__, "Failed to get library at address 0x%lx\n", addr);
        snprintf(outBuf, BUFSIZE, "?");
        return STAT_OK;
    }
    boolRet = Symtab::openFile(symtab, lib.first);
    if (boolRet == false || symtab == NULL)
    {
        printMsg(STAT_WARNING, __FILE__, __LINE__, "Symtab failed to open file %s\n", lib.first.c_str());
        snprintf(outBuf, BUFSIZE, "?");
        return STAT_OK;
    }
    symtab->setTruncateLinePaths(false);
    loadAddr = lib.second;
    boolRet = symtab->getSourceLines(lines, addr - loadAddr);
    if (boolRet == false)
    {
        snprintf(outBuf, BUFSIZE, "?");
        return STAT_OK;
    }

    snprintf(outBuf, BUFSIZE, "%s", lines[0]->getFile().c_str());
    *lineNum = lines[0]->getLine();
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "address %lx resolved to %s:%d\n", addr, outBuf, *lineNum);

    return STAT_OK;
}

StatError_t STAT_BackEnd::getVariable(const Frame &frame, char *variableName, char *outBuf, unsigned int outBufSize)
{
#ifdef PROTOTYPE_TO
    int intRet, i;
    bool boolRet, found = false;
    char buf[BUFSIZE];
    string frameName;
    vector<Frame> frames;
    Function *func = NULL;
    localVar *var = NULL;
    vector<localVar *> vars;
    vector<localVar *>::iterator varsIter;

    frame.getName(frameName);
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Searching for variable %s in frame %s\n", variableName, frameName.c_str());

    frames.push_back(frame);
    func = getFunctionForFrame(frame);
    if (func == NULL)
    {
        printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to get function for frame %s\n", frameName.c_str());
        return STAT_STACKWALKER_ERROR;
    }

    for (i = 0; i < 2 && found == false; i++)
    {
        if (i == 0)
            boolRet = func->getLocalVariables(vars);
        else if (i == 1)
            boolRet = func->getParams(vars);
        if (boolRet == false)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to get local variables for frame %s\n", frameName.c_str());
            return STAT_STACKWALKER_ERROR;
        }
        for (varsIter = vars.begin(); varsIter != vars.end(); varsIter++)
        {
            var = *varsIter;
            if (strcmp(var->getName().c_str(), variableName) == 0)
            {
                intRet = getLocalVariableValue(*varsIter, frames, 0, outBuf, outBufSize);
                if (intRet != glvv_Success)
                {
                    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Failed to get variable %s for frame %s\n", variableName, frameName.c_str());
                    return STAT_STACKWALKER_ERROR;
                }
                found = true;
                break;
            }
        }
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Found variable %s in frame %s\n", outBuf, frameName.c_str());
#else
    printMsg(STAT_WARNING, __FILE__, __LINE__, "Prototype variable extraction feature not enabled!\n");
    snprintf(outBuf, BUFSIZE, "$%s=-1", variableName);
#endif
    return STAT_OK;
}

StatError_t STAT_BackEnd::mergeIntoGraphs(graphlib_graph_p currentGraph, bool isLastTrace)
{
    graphlib_error_t graphlibError;
 
    /* Merge cur graph into retGraph */
    graphlibError = graphlib_mergeGraphs(prefixTree3d_, currentGraph);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error merging graphs\n");
        return STAT_GRAPHLIB_ERROR;
    }
    if (isLastTrace == true)
    {
        graphlibError = graphlib_mergeGraphs(prefixTree2d_, currentGraph);
        if (GRL_IS_FATALERROR(graphlibError))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error merging graphs\n");
            return STAT_GRAPHLIB_ERROR;
        }
    }
 
    /* Free up current graph */
    graphlibError = graphlib_delGraph(currentGraph);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
        return STAT_GRAPHLIB_ERROR;
    }

    return STAT_OK;
}

StatError_t STAT_BackEnd::sampleStackTraces(unsigned int nTraces, unsigned int traceFrequency, unsigned int nRetries, unsigned int retryFrequency, unsigned int withThreads, unsigned int clearOnSample, char *variableSpecification, unsigned int withPython)
{
    int j;
    unsigned int i;
    static int init = 0;
    bool wasRunning = isRunning_, isLastTrace;
    static bool groupOps = false;
    graphlib_graph_p currentGraph = NULL;
    graphlib_error_t graphlibError;
    StatError_t statError;
    map<int, Walker *>::iterator processMapIter;
    map<int, StatBitVectorEdge_t *>::iterator nodeInEdgeMapIter;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Preparing to sample stack traces\n");

    /* Delete previously merged graphs */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Deleting any previously mereged trees\n");
    if (prefixTree3d_ != NULL && clearOnSample == 1)
    {
        graphlibError = graphlib_delGraph(prefixTree3d_);
        if (GRL_IS_FATALERROR(graphlibError))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
        prefixTree3d_ = NULL;
    }
    if (prefixTree2d_ != NULL)
    {
        graphlibError = graphlib_delGraph(prefixTree2d_);
        if (GRL_IS_FATALERROR(graphlibError))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
    }

    /* Create graphs */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Creating graphs\n");
    if (prefixTree3d_ == NULL)
    {
        if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
            graphlibError = graphlib_newGraph(&prefixTree3d_, statCountRepFunctions);
        else
            graphlibError = graphlib_newGraph(&prefixTree3d_, statBitVectorFunctions);
        if (GRL_IS_FATALERROR(graphlibError))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error initializing 3D graph\n");
            return STAT_GRAPHLIB_ERROR;
        }
    }
    if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
    {
        graphlibError = graphlib_newGraph(&prefixTree2d_, statCountRepFunctions);

        /* delete previous edge labels */
        for (nodeInEdgeMapIter = nodeInEdgeMap_.begin(); nodeInEdgeMapIter != nodeInEdgeMap_.end(); nodeInEdgeMapIter++)
            statFreeEdge((void *)nodeInEdgeMapIter->second);
        nodeInEdgeMap_.clear();
    }
    else
        graphlibError = graphlib_newGraph(&prefixTree2d_, statBitVectorFunctions);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error initializing 2D graph\n");
        return STAT_GRAPHLIB_ERROR;
    }

    if (init == 0)
    {
        init++;
        groupOps = doGroupOps_;
    }

//    if (withPython == true)
//        doGroupOps_ = false;
//    else
//        doGroupOps_ = groupOps;
//
//    /* Parse the variable specification */
//    nVariables_ = 0;
    if (strcmp(variableSpecification, "NULL") != 0)
    {
//        doGroupOps_ = false; /* Variable extraction currently won't work with group ops */
        statError = parseVariableSpecification(variableSpecification);
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Failed to parse variable specification\n");
            return statError;
        }
    }
//    else
//        if (withPython == false)
//            doGroupOps_ = groupOps;

    /* Gather stack traces and merge */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Gathering and merging %d traces from each task\n", nTraces);
    for (i = 0; i < nTraces; i++)
    {
        isLastTrace = (i == nTraces - 1);

        /* Pause process as necessary */
        if (isRunning_)
        {
            statError = pause();
            if (statError != STAT_OK)
                printMsg(statError, __FILE__, __LINE__, "Failed to pause processes\n");
        }

        /* Collect call stacks from each process */
#if defined(GROUP_OPS)
        if (doGroupOps_)
        {
            /* Create return graph */
            if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
                currentGraph = createRootedGraph(statCountRepFunctions);
            else
                currentGraph = createRootedGraph(statBitVectorFunctions);
            if (currentGraph == NULL)
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to create new graph\n");
                return STAT_GRAPHLIB_ERROR;
            }

            /* Get the stack trace */
            getStackTraceFromAll(currentGraph, nRetries, retryFrequency, withThreads);

            /* Merge into prefixTree2d and prefixTree3d */
            statError = mergeIntoGraphs(currentGraph, isLastTrace);
            if (statError != STAT_OK)
               return statError;
        }
        else
#endif
        {
            for (processMapIter = processMap_.begin(), j = 0; processMapIter != processMap_.end(); processMapIter++, j++)
            {
                /* Create return graph */
                if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
                    currentGraph = createRootedGraph(statCountRepFunctions);
                else
                    currentGraph = createRootedGraph(statBitVectorFunctions);
                if (currentGraph == NULL)
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to create new graph\n");
                    return STAT_GRAPHLIB_ERROR;
                }

                /* Get the stack trace */
                statError = getStackTrace(currentGraph, processMapIter->second, j, nRetries, retryFrequency, withThreads, withPython);
                if (statError != STAT_OK)
                {
                    printMsg(statError, __FILE__, __LINE__, "Error getting graph %d of %d\n", i + 1, nTraces);
                    return statError;
                }

                /* Merge into prefixTree2d and prefixTree3d */
                statError = mergeIntoGraphs(currentGraph, isLastTrace);
                if (statError != STAT_OK)
                    return statError;
            } /* for processMapIter */
        }

        /* Continue the process */
        if (isLastTrace == false || wasRunning == true)
        {
            statError = resume();
            if (statError != STAT_OK)
                printMsg(statError, __FILE__, __LINE__, "Failed to resume processes\n");
        }

        if (isLastTrace == false)
            usleep(traceFrequency * 1000);
    } /* for i */

    if (extractVariables_ != NULL)
    {
        free(extractVariables_);
        extractVariables_ = NULL;
    }

    return STAT_OK;
}


std::string STAT_BackEnd::getFrameName(const Frame &frame, int depth)
{
    int lineNum, i, value, pyLineNo;
    bool boolRet;
    char buf[BUFSIZE], fileName[BUFSIZE], *pyFun, *pySource;
    string name;
    Address addr;
    StatError_t statError;

    boolRet = frame.getName(name);
    if (boolRet == false)
        name = "[unknown]";
    else if (name == "")
        name = "[empty]";

    /* Gather Python script level traces if requested */
    if (withPython_ == true && name.find("PyEval_EvalFrameEx") != string::npos)
    {
        statError = getPythonFrameInfo(frame.getWalker(), frame, &pyFun, &pySource, &pyLineNo);
        if (statError == STAT_OK && pyFun != NULL && pySource != NULL)
        {
            if (sampleType_ == STAT_FUNCTION_AND_LINE || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
            {
                snprintf(buf, BUFSIZE, "%s@%s:%d", pyFun, pySource, pyLineNo);
                name = buf;
            }
            else
                name = pyFun;
            free(pyFun);
            free(pySource);
            isPyTrace = true;
            return name;
        }
        else if (statError != STAT_OK)
            printMsg(statError, __FILE__, __LINE__, "Error in getPythonFrameInfo for frame %s, pyFun = %s, pySource = %s\n", name.c_str(), pyFun, pySource);
    }
    
    /* Gather PC value or line number info if requested */
    if (sampleType_ == STAT_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_PC)
    {
        if (frame.isTopFrame() == true)
            snprintf(buf, BUFSIZE, "@0x%lx", frame.getRA());
        else
            snprintf(buf, BUFSIZE, "@0x%lx", frame.getRA() - 1);
        name += buf;
    }
    else if (sampleType_ == STAT_FUNCTION_AND_LINE || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
    {
        if (frame.isTopFrame() == true)
            addr = frame.getRA();
        else
            addr = frame.getRA() - 1;

        statError = getSourceLine(frame.getWalker(), addr, fileName, &lineNum);
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Failed to get source line information for %s\n", name.c_str());
            return name;
        }
        name += "@";
        name += fileName;
        if (lineNum != 0)
        {
            snprintf(buf, BUFSIZE, ":%d", lineNum);
            name += buf;
        }
        if (extractVariables_ != NULL)
        {
            for (i = 0; i < nVariables_; i++)
            {
//if (lineNum == extractVariables_[i].lineNum)            
//printf("Searching for var %s %s %s %d %d %d %d\n", name.c_str(), extractVariables_[i].fileName, fileName, extractVariables_[i].lineNum, lineNum, extractVariables_[i].depth, depth);
                if (strcmp(fileName, extractVariables_[i].fileName) == 0 &&
                    lineNum == extractVariables_[i].lineNum)//TODO: &&
//                    depth == extractVariables_[i].depth)
                {
                    statError = getVariable(frame, extractVariables_[i].variableName, buf, BUFSIZE);
                    value = *((int *)buf);
                    snprintf(buf, BUFSIZE, "$%s=%d", extractVariables_[i].variableName, value);
                    if (statError != STAT_OK)
                    {
                       printMsg(statError, __FILE__, __LINE__, "Failed to get variable information for $%s in %s\n", extractVariables_[i].variableName, name.c_str());
                       continue;
                    }
printf("%s %s\n", name.c_str(), buf);
                    name += buf;
                }
            }
        }
    }

    return name;
}

void STAT_BackEnd::fillInName(const string &name, graphlib_nodeattr_t &nodeAttr)
{
    nodeAttr.label = (char *)name.c_str();
}


StatError_t STAT_BackEnd::getStackTrace(graphlib_graph_p retGraph, Walker *proc, int rank, unsigned int nRetries, unsigned int retryFrequency, unsigned int withThreads, unsigned int withPython)
{
    int nodeId, prevId, i, j, partialVal, pos, pyLineNo;
    bool boolRet;
    char *pyFun = NULL, *pySource = NULL, outLine[BUFSIZE];
    string name, path;
    vector<Frame> swalk, partial;
    vector<string> trace, outTrace;
    graphlib_error_t graphlibError;
    StatCountRepEdge_t *countRepEdge;
    StatBitVectorEdge_t *edge;
    graphlib_nodeattr_t nodeAttr = {1,0,20,GRC_LIGHTGREY,0,0,(char *)"",GRAPH_FONT_SIZE};
    graphlib_edgeattr_t edgeAttr = {1,0,NULL,0,0,0};
    graphlib_graph_p currentGraph = NULL;
    vector<Dyninst::THR_ID> threads;
    ProcDebug *pDebug = NULL;
    StatError_t statError;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Gathering trace from task rank %d of %d\n", rank, proctabSize_);

    /* Set edge label */
    edge = initializeBitVectorEdge(proctabSize_);
    if (edge == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to initialize edge\n");
        return STAT_ALLOCATE_ERROR;
    }
    edge->bitVector[rank / STAT_BITVECTOR_BITS] |= STAT_GRAPH_BIT(rank % STAT_BITVECTOR_BITS);
    if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
    {
        countRepEdge = (StatCountRepEdge_t *)malloc(sizeof(StatCountRepEdge_t));
        if (countRepEdge == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to initialize edge\n", strerror(errno));
            return STAT_ALLOCATE_ERROR;
        }
        countRepEdge->count = 1;
        countRepEdge->representative = proctab_[rank].mpirank;
        countRepEdge->checksum = proctab_[rank].mpirank + 1;
        edgeAttr.label = (void *)countRepEdge;
    }
    else
        edgeAttr.label = (void *)edge;

    /* If we're not attached return a trace denoting the task has exited */
    if (proc == NULL)
        threads.push_back(NULL_THR_ID);
    else
    {
        pDebug = dynamic_cast<ProcDebug *>(proc->getProcessState());
        if (pDebug == NULL)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Walker contains no Process State object\n");
            return STAT_STACKWALKER_ERROR;
        }
        else
        {
            if (pDebug->isTerminated() || withThreads == 0)
                threads.push_back(NULL_THR_ID);
            else
            {
                boolRet = proc->getAvailableThreads(threads);
                if (boolRet != true)
                {
                    printMsg(STAT_WARNING, __FILE__, __LINE__, "Get threads failed... using null thread id instead\n");
                    threads.push_back(NULL_THR_ID);
                }
            }
        }
    }

    /* Loop over the threads and get the traces */
    for (j = 0; j < threads.size(); j++)
    {
        /* Initialize loop iteration specific variables */
        trace.clear();
        prevId = 0;
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Gathering trace from thread %d of %d\n", j, threads.size());

        /* Create current working graph */
        if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
           currentGraph = createRootedGraph(statCountRepFunctions);
        else
           currentGraph = createRootedGraph(statBitVectorFunctions);
        if (currentGraph == NULL)
        {
           printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to create new graph\n");
           return STAT_GRAPHLIB_ERROR;
        }

        path = "";
        if (proc == NULL)
            trace.push_back("[task_exited]"); /* We're not attached so return a trace denoting the task has exited */
        else
        {
            /* Get the stack trace */
            partialVal = 0;
            for (i = 0; i <= nRetries; i++)
            {
                /* Try to walk the stack */
                swalk.clear();
                boolRet = proc->walkStack(swalk, threads[j]);
                if (boolRet == false && swalk.size() < 1)
                {
                   printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "RETRY failed walk, on attempt %d of %d, thread %d id %d with StackWalker error %s\n", i, nRetries, j, threads[j], Stackwalker::getLastErrorMsg());
                    if (i < nRetries)
                    {
                        if (isRunning_ == false)
                            resumeImpl(proc);
                        usleep(retryFrequency);
                        if (isRunning_ == false)
                            pauseImpl(proc);
                    }
                    continue;
                }

                /* Get the name of the top frame */
                boolRet = swalk[swalk.size() - 1].getName(name);
                if (boolRet == false)
                {
                    if (partialVal < 1)
                    {
                        /* This is the best trace so far */
                        partialVal = 1;
                        partial = swalk;
                    }
                    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "RETRY: top of stack = unknown frame on attempt %d of %d\n", i, nRetries);
                    if (i < nRetries)
                    {
                        if (isRunning_ == false)
                            resumeImpl(proc);
                        usleep(retryFrequency);
                        if (isRunning_ == false)
                            pauseImpl(proc);
                    }
                    continue;
                }

                /* Make sure the top frame contains the string "start" (for main thread only) */
                if (name.find("start", 0) == string::npos && j == 0)
                {
                    if (partialVal < 2)
                    {
                        /* This is the best trace so far */
                        partialVal = 2;
                        partial = swalk;
                    }
                    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "RETRY: top of stack '%s' not start on attempt %d of %d\n", name.c_str(), i, nRetries);
                    if (i < nRetries)
                    {
                        if (isRunning_ == false)
                            resumeImpl(proc);
                        usleep(retryFrequency);
                        if (isRunning_ == false)
                            pauseImpl(proc);
                    }
                    continue;
                }

                /* The trace looks good so we'll use this one */
                partial = swalk;
                break;
            }

            /* Check to see if we got a complete stack trace */
            if (i > nRetries && partialVal < 1)
            {
               printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "StackWalker reported an error in walking the stack: %s\n", Stackwalker::getLastErrorMsg());
                trace.push_back("StackWalker_Error");
            }
            else
            {
                /* Get the name of each frame */
                isFirstPythonFrame = true;
                isPyTrace = false;
                for (i = partial.size() - 1; i >= 0; i = i - 1)
                {
                    name = getFrameName(partial[i], partial.size() - i + 1);

                    /* Add the node and edge to the graph */
                    if (withPython != 0)
                    {
                        if (isPyTrace == true && isFirstPythonFrame == true)
                        {
                            /* This is our new root, drop preceding frames */
                            trace.clear();
                            isFirstPythonFrame = false;
                            trace.push_back(name);
                            continue;
                        }
        
                        if (name.find("PyCFunction_Call") != string::npos)
                        {
                            /* This indicates the end of the Python interpreter */
                            continue;
                        }
                        if (isPyTrace == true && (name.find("call_function") == string::npos && name.find("fast_function") == string::npos && name.find("PyEval") == string::npos))
                            trace.push_back(name);
                        else if (isPyTrace == false)
                            trace.push_back(name);
                    }
                    else
                        trace.push_back(name);
                }
            }
        } /* else proc == NULL */

        /* Get the name of the frames and add it to the graph */
        for (i = 0; i < trace.size(); i++)
        {
            /* Add \ character before '<' and '>' characters for dot format */
            pos = -2;
            while (1)
            {
                pos = trace[i].find('<', pos + 2);
                if (pos == string::npos)
                    break;
                trace[i].insert(pos, "\\");
            }
            pos = -2;
            while (1)
            {
                pos = trace[i].find('>', pos + 2);
                if (pos == string::npos)
                    break;
                trace[i].insert(pos, "\\");
            }

//            /* Add the node and edge to the graph */
//            if (withPython != 0)
//            {
//printf("PYTHON %s\n", trace[i].c_str());            
//                if (trace[i].find("PyCFunction_Call") != string::npos)
//                {
//                    /* This indicates the end of the Python interpreter */
//                    isPyTrace = false;
//                    continue;
//                }
//                if (isPyTrace == true && (trace[i].find("call_function") == string::npos && trace[i].find("fast_function") == string::npos && trace[i].find("PyEval") == string::npos))
//                {
//                    /* This is to handle optimized builds that don't go through the PyCFunction_Call frame, but rather go into C modules directly from PyEval_EvalFrameEx */
//                    isPyTrace = false;
//                    continue;
//                }
//                if (isPyTrace == true && isFirstPythonFrame == true)
//                {
//                    outTrace.clear();
//                    isFirstPythonFrame = false;
//                }
//                if (isPyTrace == true)
//                    outTrace.push_back(trace[i]);
//                //if (trace[i].find("PyEval_EvalFrameEx") != string::npos)
//                //{
//                //    statError = getPythonFrameInfo(proc, swalk[trace.size() - 1 - i], &pyFun, &pySource, &pyLineNo);
//                //    if (statError == STAT_OK && pyFun != NULL && pySource != NULL)
//                //    {
//                //        if (isPyTrace == false)
//                //        {
//                //            outTrace.clear();
//                //            isPyTrace = true;
//                //        }
//                //        if (trace[i].find("@") != string::npos)
//                //        {
//                //            snprintf(outLine, BUFSIZE, "%s@%s:%d", pyFun, pySource, pyLineNo);
//                //            trace[i] = outLine;
//                //        }
//                //        else
//                //            trace[i] = pyFun;
//                //        outTrace.push_back(trace[i]);
//                //        free(pyFun);
//                //        free(pySource);
//                //    }
//                //    else if (statError != STAT_OK)
//                //        printMsg(statError, __FILE__, __LINE__, "Error in getPythonFrameInfo for frame %s, pyFun = %s, pySource = %s\n", trace[i].c_str(), pyFun, pySource);
//                //}
//            }
//            if (isPyTrace == false)
                outTrace.push_back(trace[i]);
        }

        for (i = 0; i < outTrace.size(); i++)
        {
            path += outTrace[i].c_str();
            nodeId = statStringHash(path.c_str());
            fillInName(outTrace[i], nodeAttr);
            graphlibError = graphlib_addNode(currentGraph, nodeId, &nodeAttr);

            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error adding node to graph\n");
                return STAT_GRAPHLIB_ERROR;
            }
            graphlibError = graphlib_addDirectedEdge(currentGraph, prevId, nodeId, &edgeAttr);
            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error adding edge to graph\n");
                return STAT_GRAPHLIB_ERROR;
            }

            if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
            {
                if (nodeInEdgeMap_.find(nodeId) != nodeInEdgeMap_.end())
                    statMergeEdge(nodeInEdgeMap_[nodeId], edge);
                else
                    nodeInEdgeMap_[nodeId] = (StatBitVectorEdge_t *)statCopyEdge((void *)edge);
            }
            prevId = nodeId;
        }
        outTrace.clear();

        /* Merge cur graph into retGraph */
        graphlibError = graphlib_mergeGraphs(retGraph, currentGraph);
        if (GRL_IS_FATALERROR(graphlibError))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error merging graphs\n");
            return STAT_GRAPHLIB_ERROR;
        }

        /* Free up current graph */
        graphlibError = graphlib_delGraph(currentGraph);
        if (GRL_IS_FATALERROR(graphlibError))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
            return STAT_GRAPHLIB_ERROR;
        }
        
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Trace from thread %d of %d = %s\n", j, threads.size(), path.c_str());

    } /* for j thread */

    if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
    {
        graphlibError = graphlib_delEdgeAttr(edgeAttr, statFreeCountRepEdge);
        statFreeEdge(edge);
    }
    else
        graphlibError = graphlib_delEdgeAttr(edgeAttr, statFreeEdge);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to free edge attr\n");
        return STAT_GRAPHLIB_ERROR;
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Trace successfully gathered from task rank %d\n", rank);
    return STAT_OK;
}

#if defined(GROUP_OPS)
StatError_t STAT_BackEnd::addFrameToGraph(graphlib_graph_p graphlibGraph, CallTree *stackwalkerGraph, graphlib_node_t graphlibNode, FrameNode *stackwalkerNode, string nodeIdNames, set<pair<Walker *, THR_ID> > *errorThreads, set<int> &outputRanks)
{
    int i, rank, newChildId;
    set<int> myRanks, kidsRanks;
    set<int>::iterator myRanksIter;
    string name, newNodeIdNames;
    frame_set_t &children = stackwalkerNode->getChildren();
    frame_set_t::iterator childrenIter;
    Frame *frame;
    vector<Frame> frames;
    FrameNode *child;
    set<FrameNode *> kids;
    set<FrameNode *>::iterator kidsIter;
    map<string, set<FrameNode*> > childrenNames;
    map<string, set<FrameNode*> >::iterator childrenNamesIter;
    THR_ID threadId;
    Walker *walker;
    map<Walker *, int>::iterator procsToRanksIter;
    graphlib_error_t graphlibError;
    graphlib_nodeattr_t nodeAttr = {1,0,20,GRC_LIGHTGREY,0,0, (char *)"", GRAPH_FONT_SIZE};
    graphlib_edgeattr_t edgeAttr = {1,0,NULL,0,0,0};
    StatBitVectorEdge_t *edge;
    StatCountRepEdge_t *countRepEdge;
    StatError_t statError;

    /* Add the Frame associated with stackwalkerNode to the graphlibGraph, below the given graphlibNode */
    /* Note: Lots of complexity here to deal with adding edge labels (all the std::set work). We could get rid of this if graphlib had graph traversal functions. */
    /* TODO: how to handle Python? */

    /* Traversal 1: Get names for each child in the FrameNode graph.  Merge like-names together into the childrenNames map. */
    for (childrenIter = children.begin(); childrenIter != children.end(); childrenIter++)
    {
        child = *childrenIter;
        frame = child->getFrame();
        if (frame == NULL)
        {
            /* We hit a thread, that means the end of the callstack. Add the associated rank to our edge label set. */
            threadId = child->getThread();
            if (threadId == NULL_LWP)
            {
                printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Thread ID is NULL\n");
                return STAT_STACKWALKER_ERROR;
            }
            walker = child->getWalker();
            if (walker == NULL)
            {
                printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Walker is NULL\n");
                return STAT_STACKWALKER_ERROR;
            }

            if (errorThreads && child->hadError())
            {
                /* This stackwalk ended in an error.  Don't add it to the tree, instead return it via errorThreads.  This may lead to edges with no associated ranks, those are cleaned below. */
                errorThreads->insert(make_pair(walker, threadId));
                continue;
            }

            procsToRanksIter = procsToRanks_.find(walker);
            if (procsToRanksIter == procsToRanks_.end())
            {
                printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed fo find walker in procsToRanks_ map\n");
                return STAT_STACKWALKER_ERROR;
            }
            rank = procsToRanksIter->second;
            outputRanks.insert(rank);
            continue;
        }
        name = getFrameName(*frame);
//        if ((isPyTrace == true && (name.find("call_function") == string::npos && name.find("fast_function") == string::npos && name.find("PyEval") == string::npos)) || isPyTrace == false)
//        {
            childrenNames[name].insert(child);
            frames.insert(frames.begin(), *frame);
//        }
//        else
//            children.erase(childrenIter);
    }
//    for (i = frames.size() - 1, childrenIter = children.begin(); i >= 0; i = i - 1, childrenIter++)
//    {
//        child = *childrenIter;
//        name = getFrameName(frames[i]);
//        if (isPyTrace == true && (name.find("call_function") == string::npos && name.find("fast_function") == string::npos && name.find("PyEval") == string::npos))
//            childrenNames[name].insert(child);
//        else if (isPyTrace == false)
//            childrenNames[name].insert(child);
//    }

    /* Traversal 2: For each unique name, create a new graphlib node and add it */
    for (childrenNamesIter = childrenNames.begin(); childrenNamesIter != childrenNames.end(); childrenNamesIter++)
    {
        name = childrenNamesIter->first;
        kids = childrenNamesIter->second;

        newNodeIdNames = nodeIdNames + name;
        newChildId = statStringHash(newNodeIdNames.c_str());

        if (stackwalkerNode->isHead())
            newChildId = graphlibNode; /* Don't create a node for head, just using existing node. */
        else 
        {
            fillInName(name, nodeAttr);
            graphlibError = graphlib_addNode(graphlibGraph, newChildId, &nodeAttr);
            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add node\n");
                continue;
            }
         }

        /* Traversal 2.1: For each new node, recursively add its children to the graph */
        for (kidsIter = kids.begin(); kidsIter != kids.end(); kidsIter++)
        {
            child = *kidsIter;
            statError = addFrameToGraph(graphlibGraph, stackwalkerGraph, newChildId, child, newNodeIdNames, errorThreads, kidsRanks);
            if (statError != STAT_OK)
            {
                printMsg(statError, __FILE__, __LINE__, "Error adding frame to graph\n");
                return statError;
            }
            if (kidsRanks.empty())
            {
                printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "kidsRanks is empty\n");
                return STAT_STACKWALKER_ERROR;
            }
            myRanks.insert(kidsRanks.begin(), kidsRanks.end());
        }

        if (myRanks.empty())
        {
            /* This node only had error stackwalks ending below it, thus no ranks. It doesn't need to be included. */
            graphlibError = graphlib_deleteConnectedNode(graphlibGraph, newChildId);
            if (GRL_IS_FATALERROR(graphlibError))
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to cleanup nodes\n");
            continue;
        }

        if (graphlibNode != newChildId)
        {
            /* Create the edge between this new node and our parent */
            edge = initializeBitVectorEdge(proctabSize_);
            if (edge == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to initialize edge\n");
                return STAT_ALLOCATE_ERROR;
            }
            for (myRanksIter = myRanks.begin(); myRanksIter != myRanks.end(); myRanksIter++)
            {
                rank = *myRanksIter;
                edge->bitVector[rank / STAT_BITVECTOR_BITS] |= STAT_GRAPH_BIT(rank % STAT_BITVECTOR_BITS);
            }
            if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
            {
                countRepEdge = (StatCountRepEdge_t *) malloc(sizeof(StatCountRepEdge_t));
                if (countRepEdge == NULL)
                {
                     printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to initialize edge\n", strerror(errno));
                     return STAT_ALLOCATE_ERROR;
                }
                countRepEdge->count = 0;
                countRepEdge->representative = -1;
                countRepEdge->checksum = 0;
                for (myRanksIter = myRanks.begin(); myRanksIter != myRanks.end(); myRanksIter++)
                {
                    rank = *myRanksIter;
                    if (rank < countRepEdge->representative || countRepEdge->representative == -1)
                        countRepEdge->representative = proctab_[rank].mpirank;
                    countRepEdge->checksum += proctab_[rank].mpirank + 1; /* We add one to ensure rank == 0 gets counted */
                    countRepEdge->count += 1;
                }
                edgeAttr.label = (void *)countRepEdge;
            }
            else
                edgeAttr.label = (void *)edge;
            graphlibError = graphlib_addDirectedEdge(graphlibGraph, graphlibNode, newChildId, &edgeAttr);
            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add edge\n");
                continue;
            }

            if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
            {
                if (nodeInEdgeMap_.find(newChildId) != nodeInEdgeMap_.end())
                    statMergeEdge(nodeInEdgeMap_[newChildId], edge);
                else
                    nodeInEdgeMap_[newChildId] = (StatBitVectorEdge_t *)statCopyEdge((void *)edge);
            }
            if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
            {
                graphlibError = graphlib_delEdgeAttr(edgeAttr, statFreeCountRepEdge);
                statFreeEdge(edge);
            }
            else
                graphlibError = graphlib_delEdgeAttr(edgeAttr, statFreeEdge);
        } /* if (graphlibNode != newChildId) */
        outputRanks.insert(myRanks.begin(), myRanks.end());
    }
    return STAT_OK;
}

STAT_BackEnd *bePtr;
bool statFrameCmp(const Frame &frame1, const Frame &frame2)
{
    string name1, name2;

    name1 = bePtr->getFrameName(frame1, -1);
    name2 = bePtr->getFrameName(frame2, -1);
    if (name1.compare(name2) < 0)
        return true;
    return false;
}

StatError_t STAT_BackEnd::getStackTraceFromAll(graphlib_graph_p retGraph, unsigned int nRetries, unsigned int retryFrequency, unsigned int withThreads)
{
    unsigned int i;
    bool boolRet;
    string empty;
    set<int> ranks;
    StatError_t statError;
    graphlib_edgeattr_t edgeAttr = {1,0,NULL,0,0,0};

    if (procSet_->getLWPTracking() && withThreads)
        procSet_->getLWPTracking()->refreshLWPs();

    /* TODO: change comparator to do variables (also Python?) */
    /* cache values to avoid recomputing since this is performed many times */
    //CallTree tree(frame_lib_offset_cmp);
    bePtr = this;
    CallTree tree(statFrameCmp);
    boolRet = walkerSet_->walkStacks(tree, !withThreads);
    if (boolRet == false) 
    {
#warning TODO: Start handling partial call stacks in group walks
    }

    isPyTrace = false;
    isFirstPythonFrame = true;
    statError = addFrameToGraph(retGraph, &tree, 0, tree.getHead(), empty, NULL, ranks);
    if (statError != STAT_OK) 
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to getStackTraceFromAll\n");
        return statError;
    }

    return STAT_OK;
}
#endif

StatError_t STAT_BackEnd::detach(unsigned int *stopArray, int stopArrayLen)
{
    int i;
    bool leaveStopped;
    map<int, Walker *>::iterator processMapIter;
    ProcDebug *pDebug;
    
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Detaching from application processes\n");

#if defined(GROUP_OPS)
    if (doGroupOps_)
        procSet_->detach();
    else
#endif
    {
        for (processMapIter = processMap_.begin(); processMapIter != processMap_.end(); processMapIter++)
        {
            leaveStopped = false;
            if (processMapIter->second != NULL)
            {
                pDebug = dynamic_cast<ProcDebug *>(processMapIter->second->getProcessState());
                if (pDebug == NULL)
                {
                    printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to dynamic_cast ProcDebug pointer\n");
                    return STAT_STACKWALKER_ERROR;
                }
#ifndef BGL
                for (i = 0; i < stopArrayLen; i++)
                {
                    if (stopArray[i] == processMapIter->first)
                    {
                        leaveStopped = true;
                        break;
                    }
                }
#endif
                pDebug->detach(leaveStopped);
                delete processMapIter->second;
            }
            processMap_.erase(processMapIter);
            processMapNonNull_ = processMapNonNull_ - 1;
        }
    }
    return STAT_OK;
}

StatError_t STAT_BackEnd::terminateApplication()
{
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Terminating application processes\n");
#warning TODO: Fill in terminateApplication

    return STAT_OK;
}

StatError_t STAT_BackEnd::sendAck(Stream *stream, int tag, int val)
{
   if (stream->send(tag, "%d", val) == -1)
   {
       printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send reply failed\n");
       return STAT_MRNET_ERROR;
   }

   if (stream->flush() == -1)
   {
       printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::flush() failure\n");
       return STAT_MRNET_ERROR;
   }

   return STAT_OK;
}

int unpackStatBeInfo(void *buf, int bufLen, void *data)
{
    StatLeafInfoArray_t *leafInfoArray = (StatLeafInfoArray_t *)data;
    int i, j, nChildren, nParents, k, currentParentPort, currentParentRank;
    char *ptr = (char *)buf, currentParent[STAT_MAX_BUF_LEN];

    /* Get the number of daemons and set up the leaf info array */
    memcpy((void *)&(leafInfoArray->size), ptr, sizeof(int));
    ptr += sizeof(int);
    leafInfoArray->leaves = (StatLeafInfo_t *)malloc(leafInfoArray->size * sizeof(StatLeafInfo_t));
    if (leafInfoArray->leaves == NULL)
    {
        fprintf(stderr, "Master daemon failed to allocate leaf info array\n");
        return -1;
    }

    /* Get MRNet parent node info for each daemon */
    memcpy((void *)&nParents, ptr, sizeof(int));
    ptr += sizeof(int);
    j = -1;
    for (i = 0; i < nParents; i++)
    {
        /* Get the parent host name, port, rank and child count */
        strncpy(currentParent, ptr, STAT_MAX_BUF_LEN);
        ptr += strlen(currentParent) + 1;
        memcpy(&currentParentPort, ptr, sizeof(int));
        ptr += sizeof(int);
        memcpy(&currentParentRank, ptr, sizeof(int));
        ptr += sizeof(int);
        memcpy(&nChildren, ptr, sizeof(int));
        ptr += sizeof(int);

        /* Iterate over this parent's children */
        for (k = 0; k < nChildren; k++)
        {
            j++;
            if (j >= leafInfoArray->size)
            {
                fprintf(stderr, "Failed to unpack STAT info from the front end.  Expecting only %d daemons, but received %d\n", leafInfoArray->size, j);
                return -1;
            }

            /* Fill in the parent information */
            strncpy((leafInfoArray->leaves)[j].parentHostName, currentParent, STAT_MAX_BUF_LEN);
            (leafInfoArray->leaves)[j].parentRank = currentParentRank;
            (leafInfoArray->leaves)[j].parentPort = currentParentPort;

            /* Get the daemon host name */
            strncpy((leafInfoArray->leaves)[j].hostName, ptr, STAT_MAX_BUF_LEN);
            ptr += strlen((leafInfoArray->leaves)[j].hostName) + 1;

            /* Get the daemon rank */
            memcpy(&((leafInfoArray->leaves)[j].rank), ptr, sizeof(int));
            ptr += sizeof(int);
        }
    } /* for i */

    return 0;
}

StatError_t STAT_BackEnd::startLog(char *logOutDir, bool useMrnetPrintf, int mrnetOutputLevel)
{
    char fileName[BUFSIZE];

    useMrnetPrintf_ = useMrnetPrintf;
    snprintf(fileName, BUFSIZE, "%s/%s.STATD.log", logOutDir, localHostName_);
    statOutFp = fopen(fileName, "w");
    if (statOutFp == NULL)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: fopen failed for %s\n", strerror(errno), fileName);
        return STAT_FILE_ERROR;
    }
    if (useMrnetPrintf_ == true)
        mrn_printf_init(statOutFp);
    set_OutputLevel(mrnetOutputLevel);

    return STAT_OK;
}

void STAT_BackEnd::printMsg(StatError_t statError, const char *sourceFile, int sourceLine, const char *fmt, ...)
{
    va_list args;
    char timeString[BUFSIZE], msg[BUFSIZE];
    const char *timeFormat = "%b %d %T";
    time_t currentTime;
    struct tm *localTime;

    /* If this is a log message and we're not logging, then return */
    if (statError == STAT_LOG_MESSAGE && statOutFp == NULL)
        return;

    /* Get the time */
    currentTime = time(NULL);
    localTime = localtime(&currentTime);
    if (localTime == NULL)
        snprintf(timeString, BUFSIZE, "NULL");
    else
        strftime(timeString, BUFSIZE, timeFormat, localTime);

    if (statError != STAT_LOG_MESSAGE && statError != STAT_WARNING)
    {
        /* Print the error to the screen (or designated error file) */
        fprintf(errOutFp_, "<%s> <%s:%d> ", timeString, sourceFile, sourceLine);
        fprintf(errOutFp_, "%s: STAT returned error type ", localHostName_);
        statPrintErrorType(statError, errOutFp_);
        fprintf(errOutFp_, ": ");
        va_start(args, fmt);
        vfprintf(errOutFp_, fmt, args);
        va_end(args);
    }
    else if (statError == STAT_WARNING)
    {
        /* Print the warning message to error output */
        fprintf(errOutFp_, "<%s> <%s:%d> ", timeString, sourceFile, sourceLine);
        fprintf(errOutFp_, "%s: STAT_WARNING: ", localHostName_);
        va_start(args, fmt);
        vfprintf(errOutFp_, fmt, args);
        va_end(args);
    }

    /* Print the message to the log */
    if (statOutFp != NULL)
    {
        if (useMrnetPrintf_ == true)
        {
            va_start(args, fmt);
            vsnprintf(msg, BUFSIZE, fmt, args);
            va_end(args);
            mrn_printf(sourceFile, sourceLine, "", statOutFp, "%s", msg);
        }
        else
        {
            fprintf(statOutFp, "<%s> <%s:%d> ", timeString, sourceFile, sourceLine);
            if (statError != STAT_LOG_MESSAGE && statError != STAT_STDOUT && statError != STAT_VERBOSITY)
            {
                fprintf(statOutFp, "%s: STAT returned error type ", localHostName_);
                statPrintErrorType(statError, statOutFp);
                fprintf(statOutFp, ": ");
            }
            va_start(args, fmt);
            vfprintf(statOutFp, fmt, args);
            va_end(args);
            fflush(statOutFp);
        }
    }
}

vector<Field *> *STAT_BackEnd::getComponents(Type *type)
{
    typeTypedef *tt;
    typeStruct *ts;
    vector<Field *> *components;

    tt = type->getTypedefType();
    if (tt == NULL)
    {
        printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "getTypeDefType returned NULL\n");
        return NULL;
    }
    
    type = tt->getConstituentType();
    if (type == NULL)
    {
        printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "getConstituentType returned NULL\n");
        return NULL;
    }
    
    ts = type->getStructType();
    if (ts == NULL)
    {
        printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "getStructType returned NULL\n");
        return NULL;
    }
    
    components = ts->getComponents();
    if (components == NULL)
    {
        printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "getComponents returned NULL\n");
        return NULL;
    }
    return components;
}

StatError_t STAT_BackEnd::getPythonFrameInfo(Walker *proc, const Frame &frame, char **pyFun, char **pySource, int *pyLineNo)
{
#ifdef PROTOTYPE_PY
    int i, found = 0, fLastIVal = -1, intRet, address, lineNo, firstLineNo;
    static int coNameOffset = -1, coFileNameOffset = -1, obSizeOffset = -1, obSvalOffset = -1, coFirstLineNoOffset = -1, coLnotabOffset = -1, fLastIOffset = -1, pyVarObjectObSizeOffset = -1, pyBytesObjectObSvalOffset = -1;
    unsigned int j;
    long length, pAddr;
    unsigned long long baseAddr, pyCodeObjectBaseAddr, pyUnicodeObjectAddr;
    bool boolRet; 
    static bool isUnicode = false, isPython3 = false;
    char buffer[BUFSIZE], varName[BUFSIZE];
    Function *function;
    Symtab *symtab = NULL;
    localVar *var;
    vector<localVar *> vars;
    vector<localVar *>::iterator varsIter;
    Type *type;
    vector<Field *> *components;
    Field *field;
    LibraryPool::iterator libsIter;
    ProcDebug *pDebug = NULL;
    StatError_t statError;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Attempting to get Python frame information\n");

    /* Find all necessary variable field offsets */
    if (coNameOffset == -1 || coFileNameOffset == -1 || obSizeOffset == -1 || obSvalOffset == -1 || coFirstLineNoOffset == -1 || coLnotabOffset == -1 || fLastIOffset == -1)
    {
        boolRet = Symtab::openFile(symtab, proctab_[0].pd.executable_name);
        if (boolRet == false)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "openFile returned false for %s\n", proctab_[0].pd.executable_name);
            return STAT_STACKWALKER_ERROR;
        }

        /* PyFrameObject used for line number calculation */
        boolRet = symtab->findType(type, "PyFrameObject");
        if (boolRet == false)
        { 
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "PyFrameObject not found in %s\n", proctab_[0].pd.executable_name);
#ifdef SW_VERSION_8_0_0
            /* Look for libpython* and then search for PyFrameObject in there */
            found = 0;
            pDebug = static_cast<ProcDebug *>(proc->getProcessState());
            Process::ptr proccessPtr = pDebug->getProc();
            LibraryPool &libs = proccessPtr->libraries();
            for (libsIter = libs.begin(); libsIter != libs.end(); libsIter++)
            {
                Library::ptr libraryPtr = *libsIter;
                if (libraryPtr->getName().find("libpython") == string::npos)
                    continue;
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Searching for PyFrameObject in %s\n", libraryPtr->getName().c_str());
                boolRet = Symtab::openFile(symtab, libraryPtr->getName().c_str());
                if (boolRet == false)
                    continue;
                boolRet = symtab->findType(type, "PyFrameObject");
                if (boolRet == true)
                {
                    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Found PyFrameObject in %s\n", libraryPtr->getName().c_str());
                    found = 1;
                    break;
                }
            }
            if (found == 0)
            {
                printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to find PyFrameObject in libraries\n");
                return STAT_STACKWALKER_ERROR;
            }
#else
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "findType returned false for PyFrameObject\n");
            return STAT_STACKWALKER_ERROR;
#endif
        }
      
        components = getComponents(type);
        if (components == NULL)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "getComponents returned NULL\n");
            return STAT_STACKWALKER_ERROR;
        }
      
        for (j = 0; j < components->size(); j++)
        {
            field = (*components)[j];
            if (strcmp(field->getName().c_str(), "f_lasti") == 0)
            {
                fLastIOffset = field->getOffset() / 8;
                break;
            }
        }

        /* PyCodeObject used for line number calculation and for various name fields */
        boolRet = symtab->findType(type, "PyCodeObject");
        if (boolRet == false)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "findType returned NULL for PyCodeObject\n");
            return STAT_STACKWALKER_ERROR;
        }
     
        components = getComponents(type);
        if (components == NULL)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "getComponents returned NULL\n");
            return STAT_STACKWALKER_ERROR;
        }
     
        found = 0;
        for (j = 0; j < components->size(); j++)
        {
            if (found == 4)
                break;
            field = (*components)[j];
            if (strcmp(field->getName().c_str(), "co_firstlineno") == 0)
            {
                coFirstLineNoOffset = field->getOffset() / 8;
                found++;
            }
            else if (strcmp(field->getName().c_str(), "co_lnotab") == 0)
            {
                coLnotabOffset = field->getOffset() / 8;
                found++;
            }
            else if (strcmp(field->getName().c_str(), "co_filename") == 0)
            {
                coFileNameOffset = field->getOffset() / 8;
                found++;
            }
            else if (strcmp(field->getName().c_str(), "co_name") == 0)
            {
                coNameOffset = field->getOffset() / 8;
                found++;
            }
        }
        
        /* PyStringObject used in Pyton 2.X for name container */
        boolRet = symtab->findType(type, "PyStringObject");
        if (boolRet == false)
        {
            /* Python 3.X removed the PyStringObject type */
            /* PyUnicodeObject used in Pyton 3.2 for name container */
            boolRet = symtab->findType(type, "PyUnicodeObject");
            if (boolRet == false)
            {
                printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "findType returned false for PyStringObject and PyUnicodeObject\n");
                return STAT_STACKWALKER_ERROR;
            }
            isUnicode = true;
            isPython3 = true;
        }
     
        components = getComponents(type);
        if (components == NULL)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "getComponents returned NULL\n");
            return STAT_STACKWALKER_ERROR;
        }
     
        found = 0;
        for (j = 0; j < components->size(); j++)
        {
            if (found == 2)
                break;
            field = (*components)[j];
            if (isUnicode == true)
            {
                /* Python 3.2 */
                /* Note Python 3.3 will run through here, but won't find these fields.  We will deal with this later */
                if (strcmp(field->getName().c_str(), "str") == 0)
                {
                    obSvalOffset = field->getOffset() / 8;
                    found++;
                }
                else if (strcmp(field->getName().c_str(), "length") == 0)
                {
                    obSizeOffset = field->getOffset() / 8;
                    found++;
                }
            }
            else
            {
                /* Python 2.X */
                if (strcmp(field->getName().c_str(), "ob_sval") == 0)
                {
                    obSvalOffset = field->getOffset() / 8;
                    found++;
                }
                else if (strcmp(field->getName().c_str(), "ob_size") == 0)
                {
                    obSizeOffset = field->getOffset() / 8;
                    found++;
                }
            }
        } /* for j */
    } /* if any static field == -1, i.e., has not been set yet */

    if (obSvalOffset == -1 and obSizeOffset == -1 and isUnicode == true)
    {
        /* Python 3.3 uses PyASCIIObject string for name container */
        boolRet = symtab->findType(type, "PyASCIIObject");
        if (boolRet == false)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "findType returned false for PyASCIIObject\n");
            return STAT_STACKWALKER_ERROR;
        }
     
        components = getComponents(type);
        if (components == NULL)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "getComponents returned NULL\n");
            return STAT_STACKWALKER_ERROR;
        }

        for (j = 0; j < components->size(); j++)
        {
            field = (*components)[j];
            if (strcmp(field->getName().c_str(), "length") == 0)
            {
                obSizeOffset = field->getOffset() / 8;
                break;
            }
        }
        obSvalOffset = type->getSize() + 4; /* string right after object, not exactly sure why + 4 necessary */
        isUnicode = false; /* the data is stored as regular C string in this case */
    }

    if (isPython3 == true && pyVarObjectObSizeOffset == -1 && pyBytesObjectObSvalOffset == -1)
    {
        boolRet = symtab->findType(type, "PyVarObject");
        if (boolRet == false)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "findType returned false for PyVarObject\n");
            return STAT_STACKWALKER_ERROR;
        }

        components = getComponents(type);
        if (components == NULL)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "getComponents returned NULL\n");
            return STAT_STACKWALKER_ERROR;
        }
     
        for (j = 0; j < components->size(); j++)
        {
            field = (*components)[j];
            if (strcmp(field->getName().c_str(), "ob_size") == 0)
            {
                pyVarObjectObSizeOffset = field->getOffset() / 8;
                break;
            }
        }

        boolRet = symtab->findType(type, "PyBytesObject");
        if (boolRet == false)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "findType returned false for PyString Object and PyVarObject\n");
            return STAT_STACKWALKER_ERROR;
        }

        components = getComponents(type);
        if (components == NULL)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "getComponents returned NULL\n");
            return STAT_STACKWALKER_ERROR;
        }
     
        for (j = 0; j < components->size(); j++)
        {
            field = (*components)[j];
            if (strcmp(field->getName().c_str(), "ob_sval") == 0)
            {
                pyBytesObjectObSvalOffset = field->getOffset() / 8;
                break;
            }
        }
    } /* if is Python 3 and static fields not set */

    if (coNameOffset == -1 || coFileNameOffset == -1 || obSizeOffset == -1 || obSvalOffset == -1 || coFirstLineNoOffset == -1 || coLnotabOffset == -1 || fLastIOffset == -1)
    {
         printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to find offsets for Python script source and line info %d %d %d %d %d %d %d\n", coNameOffset, coFileNameOffset, obSizeOffset, obSvalOffset, coFirstLineNoOffset, coLnotabOffset, fLastIOffset);
         return STAT_STACKWALKER_ERROR;
    }

    pDebug = dynamic_cast<ProcDebug *>(proc->getProcessState());
    if (pDebug == NULL)
    {
        printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "dynamic_cast returned NULL\n");
        return STAT_STACKWALKER_ERROR;
    }
    
    snprintf(varName, BUFSIZE, "f");
    statError = getVariable(frame, varName, buffer, BUFSIZE);
    if (statError != STAT_OK)
    {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "%d: Failed to get Local Variable Value for f\n", intRet);
        /* We will at least try to get the file name and return -1 for the line number */
        //return STAT_STACKWALKER_ERROR;
    }
    else
    {
        pyCodeObjectBaseAddr = *((unsigned long long *)buffer);
        pDebug->readMem(buffer, pyCodeObjectBaseAddr + fLastIOffset, sizeof(int));
        fLastIVal = *((int *)buffer);
    }

    /* Get the base address for the co variable */
    snprintf(varName, BUFSIZE, "co");
    statError = getVariable(frame, varName, buffer, BUFSIZE);
    if (statError != STAT_OK)
    {
        printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "%d: Failed to get Local Variable Value for co\n", intRet);
        return STAT_STACKWALKER_ERROR;
    }
    pyCodeObjectBaseAddr = *((unsigned long long *)buffer);

    /* Get the Python function name */
    pDebug->readMem(buffer, pyCodeObjectBaseAddr + coNameOffset, sizeof(unsigned long long));
    baseAddr = *((unsigned long long *)buffer);
    pDebug->readMem(buffer, baseAddr + obSizeOffset, sizeof(unsigned long long));
    length = *((long *)buffer);
    if (length > BUFSIZE)
        length = BUFSIZE - 1;
    if (isUnicode == true)
    {
        pDebug->readMem(buffer, baseAddr + obSvalOffset, sizeof(unsigned long long));
        pyUnicodeObjectAddr = *((unsigned long long *)buffer);
        /* It appears that we can read every other byte to get the C string representation of the unicode */
        for (i = 0; i < length; i++)
            pDebug->readMem(&buffer[i], pyUnicodeObjectAddr + 2 * i, 1);
        buffer[length] = '\0';
        *pyFun = strdup(buffer);

    }
    else
    {
        pDebug->readMem(buffer, baseAddr + obSvalOffset, length);
        buffer[length] = '\0';
        *pyFun = strdup(buffer);
    }
    if (*pyFun == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed on call to strdup(%s) to *pyFun)\n", strerror(errno), buffer);
        return STAT_ALLOCATE_ERROR;
    }
    
    /* Get the Python source file name */
    pDebug->readMem(buffer, pyCodeObjectBaseAddr + coFileNameOffset, sizeof(unsigned long long));
    baseAddr = *((unsigned long long *)buffer);

    pDebug->readMem(buffer, baseAddr + obSizeOffset, sizeof(unsigned long long));
    length = *((long *)buffer);
    if (length > BUFSIZE)
        length = BUFSIZE - 1;
    if (isUnicode == true)
    {
        pDebug->readMem(buffer, baseAddr + obSvalOffset, sizeof(unsigned long long));
        pyUnicodeObjectAddr = *((unsigned long long *)buffer);
        for (i = 0; i < length; i++)
            pDebug->readMem(&buffer[i], pyUnicodeObjectAddr + 2 * i, 1);
        buffer[length] = '\0';
        *pySource = strdup(buffer);
    }
    else
    {
        pDebug->readMem(buffer, baseAddr + obSvalOffset, length);
        buffer[length] = '\0';
        *pySource = strdup(buffer);
    }
    if (*pyFun == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed on call to strdup(%s) to *pySource)\n", strerror(errno), buffer);
        return STAT_ALLOCATE_ERROR;
    }
    
    /* Get the Python source line number */
    if (fLastIVal != -1)
    {
        pDebug->readMem(buffer, pyCodeObjectBaseAddr + coFirstLineNoOffset, sizeof(unsigned long long));
        baseAddr = *((unsigned long long *)buffer);
        pDebug->readMem(buffer, baseAddr + obSvalOffset, sizeof(int));
        firstLineNo = *((int *)buffer);
        pDebug->readMem(buffer, pyCodeObjectBaseAddr + coLnotabOffset, sizeof(unsigned long long));
        baseAddr = *((unsigned long long *)buffer);
        if (isPython3 == true)
        {
            pDebug->readMem(buffer, baseAddr + pyVarObjectObSizeOffset, sizeof(unsigned long long));
            length = *((long *)buffer);
            pAddr = baseAddr + pyBytesObjectObSvalOffset;
        }
        else
        {
            pDebug->readMem(buffer, baseAddr + obSizeOffset, sizeof(unsigned long long));
            length = *((long *)buffer);
            pAddr = baseAddr + obSvalOffset;
        }
        pDebug->readMem(buffer, pAddr, length * sizeof(unsigned char));
        address = 0;
        lineNo = firstLineNo;
        for (j = 0; j < length; j = j + 2)
        {
            address += buffer[j];
            if (address > fLastIVal)
                break;
            lineNo += buffer[j + 1];
        }
        *pyLineNo = lineNo;
    }
    else
        *pyLineNo = -1;
    
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Found Python frame information %s@%s:%d\n", *pyFun, *pySource, *pyLineNo);
#endif
 
    return STAT_OK;
}


/******************
 * STATBench Code *
 ******************/

StatError_t STAT_BackEnd::statBenchConnectInfoDump()
{
    int i, count, intRet, fd;
    unsigned int bytesWritten = 0;
    char fileName[BUFSIZE], data[BUFSIZE], *ptr;
    struct stat buf;
    string prettyHost, leafPrettyHost;
    lmon_rc_e lmonRet;
    StatLeafInfoArray_t leafInfoArray;

    /* Master daemon receive MRNet connection information */
    lmonRet = LMON_be_recvUsrData((void *)&leafInfoArray);
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to receive data from FE\n");
        return STAT_LMON_ERROR;
    }

    /* Master broadcast number of daemons */
    lmonRet = LMON_be_broadcast((void *)&(leafInfoArray.size), sizeof(int));
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to broadcast num_leaves\n");
        return STAT_LMON_ERROR;
    }

    /* Non-masters allocate space for the MRNet connection info */
    if (LMON_be_amIMaster() == LMON_NO)
    {
        leafInfoArray.leaves = (StatLeafInfo_t *)malloc(leafInfoArray.size * sizeof(StatLeafInfo_t));
        if (leafInfoArray.leaves == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to allocate memory for leaf info\n");
            return STAT_ALLOCATE_ERROR;
        }
    }

    /* Master broadcast MRNet connection information to all daemons */
    lmonRet = LMON_be_broadcast((void *)(leafInfoArray.leaves), leafInfoArray.size * sizeof(StatLeafInfo_t));
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to broadcast num_leaves\n");
        return STAT_LMON_ERROR;
    }

    /* Since we started the STATBench daemons under debugger control, */
    /* we must send a continue signal the STATBench daemons */
    for (i = 0; i < proctabSize_; i++)
        kill(proctab_[i].pd.pid, SIGCONT);

    /* Find MRNet personalities for all STATBench Daemons on this node */
    count = -1;
    for (i = 0; i < leafInfoArray.size; i++)
    {
        XPlat::NetUtils::GetHostName(localHostName_, prettyHost);
        XPlat::NetUtils::GetHostName(string(leafInfoArray.leaves[i].hostName), leafPrettyHost);
        if (prettyHost == leafPrettyHost)
        {
            /* Increment the find count and make sure it's not too large */
            count++;
            if (count >= proctabSize_)
            {
                printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "The size of the leaf information for this node appears to be larger than the process table size\n");
                return STAT_LMON_ERROR;
            }

            /* Create the named fifo for this daemon, if it doesn't already exist */
            snprintf(fileName, BUFSIZE, "/tmp/%s.%d.statbench.txt", localHostName_, count);
            while (1)
            {
                intRet = stat(fileName, &buf);
                if (intRet == 0)
                    break;
                usleep(1000);
            }

            /* Open the fifo for writing */
            fd = open(fileName, O_WRONLY);
            if (fd == -1)
            {
                printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: open() failed for %s\n", strerror(errno), fileName);
                remove(fileName);
                return STAT_FILE_ERROR;
            }

            /* Write the MRNet connection information to the fifo */
            snprintf(data, BUFSIZE, "%s %d %d %d %d", leafInfoArray.leaves[i].parentHostName, leafInfoArray.leaves[i].parentPort, leafInfoArray.leaves[i].parentRank, leafInfoArray.leaves[i].rank, proctab_[count].mpirank);
            bytesWritten = 0;
            while (bytesWritten < strlen(data))
            {
                ptr = data + bytesWritten;
                intRet = write(fd, ptr, strlen(ptr));
                if (intRet == -1)
                {
                    printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: write() to fifo %s failed\n", strerror(errno), fileName);
                    remove(fileName);
                    return STAT_FILE_ERROR;
                }
                bytesWritten += intRet;
            }

            /* Close the file descriptor */
            intRet = close(fd);
            if (intRet != 0)
            {
                printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: close() failed for fifo %s, fd %d\n", strerror(errno), fileName, fd);
                remove(fileName);
                return STAT_FILE_ERROR;
            }
        }
    }

    /* Remove the FIFOs */
    for (i = 0; i <= count; i++)
    {
        snprintf(fileName, BUFSIZE, "/tmp/%s.%d.statbench.txt", localHostName_, i);
        intRet = remove(fileName);
        if (intRet != 0)// && errno != ENOENT)
        {
            printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: remove() failed for fifo %s\n", strerror(errno), fileName);
            return STAT_FILE_ERROR;
        }
    }

    /* Free the Leaf Info array */
    if (leafInfoArray.leaves != NULL)
        free(leafInfoArray.leaves);

    return STAT_OK;
}

StatError_t STAT_BackEnd::statBenchConnect()
{
    int i, intRet, fd, bytesRead = 0, inPort, inParentRank, inRank, mpiRank;
    char fileName[BUFSIZE], inHostName[BUFSIZE], data[BUFSIZE], *ptr, *param[6], parentPort[BUFSIZE], parentRank[BUFSIZE], myRank[BUFSIZE];

    for (i = 0; i < 8192; i++)
    {
        /* Set up my file name */
        snprintf(fileName, BUFSIZE, "/tmp/%s.%d.statbench.txt", localHostName_, i);

        /* Make the fifo if the helper hasn't already done so */
        intRet = mkfifo(fileName, S_IRUSR | S_IWUSR);
        if (intRet != 0)
        {
            if (errno == EEXIST)
                continue;
            else
            {
                printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: mkfifo() failed for %s\n", strerror(errno), fileName);
                return STAT_FILE_ERROR;
            }
        }
        break;
    }

    /* Open the fifo for reading */
    fd = open(fileName, O_RDONLY);
    if (fd == -1)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: open() failed for fifo %s\n", strerror(errno), fileName);
        remove(fileName);
        return STAT_FILE_ERROR;
    }

    /* Read in my MRNet connection info */
    bytesRead = 0;
    do
    {
        ptr = data + bytesRead;
        intRet = read(fd, ptr, BUFSIZE - bytesRead);
        if (intRet == -1)
        {
            printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: read() from fifo %s fd %d failed\n", strerror(errno), fileName, fd);
            remove(fileName);
            return STAT_FILE_ERROR;
        }
        bytesRead += intRet;
    } while (intRet > 0);

    /* Extract the information from the received string */
    if (sscanf(data, "%s %d %d %d %d", inHostName, &inPort, &inParentRank, &inRank, &mpiRank) == -1)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: sscanf() failed to find MRNet connection info\n", strerror(errno));
        remove(fileName);
        return STAT_MRNET_ERROR;
    }
    parentHostName_ = strdup(inHostName);
    if (parentHostName_ == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed on call to strdup(%s) to parentHostName_)\n", strerror(errno), inHostName);
        return STAT_ALLOCATE_ERROR;
    }
    parentPort_ = inPort;
    parentRank_ = inParentRank;
    myRank_ = inRank;

    /* Close the fifo fd */
    intRet = close(fd);
    if (intRet == -1)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: close() failed for fifo %s, fd %d\n", strerror(errno), fileName, fd);
        remove(fileName);
        return STAT_FILE_ERROR;
    }

    /* Connect to the MRNet Network */
    snprintf(parentPort, BUFSIZE, "%d", parentPort_);
    snprintf(parentRank, BUFSIZE, "%d", parentRank_);
    snprintf(myRank, BUFSIZE, "%d", myRank_);
    param[0] = NULL;
    param[1] = parentHostName_;
    param[2] = parentPort;
    param[3] = parentRank;
    param[4] = localHostName_;
    param[5] = myRank;
    network_ = Network::CreateNetworkBE(6, param);
    if(network_ == NULL)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "backend_init() failed\n");
        return STAT_MRNET_ERROR;
    }

    /* Modify my rank from MRNet to MPI, since MRNet starts at 1 */
    myRank_ = mpiRank;
    connected_ = true;

    return STAT_OK;
}

StatError_t STAT_BackEnd::statBenchCreateTraces(unsigned int maxDepth, unsigned int nTasks, unsigned int nTraces, unsigned int functionFanout, int nEqClasses)
{
    static int init = 0;
    unsigned int i, j;
    graphlib_graph_p currentGraph;
    graphlib_error_t graphlibError;

    /* Initialize graphlib if we haven't already done so */
    if (init == 0)
    {
        proctabSize_ = nTasks;
        proctab_ = (MPIR_PROCDESC_EXT *)malloc(proctabSize_*sizeof(MPIR_PROCDESC_EXT));
        proctab_[0].mpirank = myRank_*nTasks;
        for (i = 0; i < proctabSize_; i++)
        {
            proctab_[i].pd.executable_name = NULL;
            proctab_[i].pd.host_name = NULL;
            proctab_[i].mpirank = proctab_[0].mpirank + i;
        }
        init++;
    }

    /* Delete previously merged graphs */
    if (prefixTree3d_ != NULL)
    {
        graphlibError = graphlib_delGraph(prefixTree3d_);
        if (GRL_IS_FATALERROR(graphlibError))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
    }
    if (prefixTree2d_ != NULL)
    {
        graphlibError = graphlib_delGraph(prefixTree2d_);
        if (GRL_IS_FATALERROR(graphlibError))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
    }

    /* Create graphs */
    if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
        graphlibError = graphlib_newGraph(&prefixTree3d_, statCountRepFunctions);
    else
        graphlibError = graphlib_newGraph(&prefixTree3d_, statBitVectorFunctions);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error initializing 3D graph\n");
        return STAT_GRAPHLIB_ERROR;
    }
    if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
        graphlibError = graphlib_newGraph(&prefixTree2d_, statCountRepFunctions);
    else
        graphlibError = graphlib_newGraph(&prefixTree2d_, statBitVectorFunctions);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error initializing 2D graph\n");
        return STAT_GRAPHLIB_ERROR;
    }

    /* Loop around the number of traces to gather */
    for (i = 0; i < nTraces; i++)
    {
        /* Loop around the number of tasks I'm emulating */
        for (j = 0; j < nTasks; j++)
        {
            /* We now create the full eq class set at once */
            if (j >= nEqClasses && nEqClasses != -1)
                break;
            /* Create the trace for this task */
            currentGraph = statBenchCreateTrace(maxDepth, j, nTasks, functionFanout, nEqClasses, i);
            if (currentGraph == NULL)
            {
                printMsg(STAT_SAMPLE_ERROR, __FILE__, __LINE__, "Error creating trace\n");
                return STAT_SAMPLE_ERROR;
            }

            /* Merge cur graph into retGraph */
            graphlibError = graphlib_mergeGraphs(prefixTree3d_, currentGraph);
            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error merging graphs\n");
                return STAT_GRAPHLIB_ERROR;
            }

            /* Merge the current graph for 2D analysis */
            if (i == 0)
            {
                graphlibError = graphlib_mergeGraphs(prefixTree2d_, currentGraph);
                if (GRL_IS_FATALERROR(graphlibError))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error merging graphs\n");
                    return STAT_GRAPHLIB_ERROR;
                }
            }

            /* Free up the temporary graph if it's not being stored as an eq class */
            if (nEqClasses == -1)
            {
                graphlibError = graphlib_delGraph(currentGraph);
                if (GRL_IS_FATALERROR(graphlibError))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
                    return STAT_GRAPHLIB_ERROR;
                }
            }
        }
    }

    return STAT_OK;
}

graphlib_graph_p STAT_BackEnd::statBenchCreateTrace(unsigned int maxDepth, unsigned int task, unsigned int nTasks, unsigned int functionFanout, int nEqClasses, unsigned int iteration)
{
    int depth, i, nodeId, prevId, currentTask;
    char temp[8192];
    static vector<graphlib_graph_p> generated_graphs;
    string path;
    graphlib_graph_p retGraph;
    graphlib_error_t graphlibError;
    graphlib_nodeattr_t nodeAttr = {1,0,20,GRC_LIGHTGREY,0,0,NULL,GRAPH_FONT_SIZE};
    StatCountRepEdge_t *countRepEdge;
    StatBitVectorEdge_t *edge;
    graphlib_edgeattr_t edgeAttr = {1,0,NULL,0,0,0};

    /* set the edge label */
    if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
    {
        countRepEdge = (StatCountRepEdge_t *)malloc(sizeof(StatCountRepEdge_t));
        if (countRepEdge == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to initialize edge\n", strerror(errno));
            return NULL;
        }
        countRepEdge->representative = proctab_[task].mpirank;
        if (nEqClasses == -1)
        {
            countRepEdge->count = 1;
            countRepEdge->checksum = proctab_[task].mpirank + 1;
        }
        else
        {
            countRepEdge->count = nTasks / nEqClasses;
            countRepEdge->checksum = 0;
            for (i = 0; i < nTasks / nEqClasses; i++)
                countRepEdge->checksum += proctab_[task].mpirank + i * nEqClasses + 1;
            if (nTasks % nEqClasses > task)
            {
                countRepEdge->count += 1;
                countRepEdge->checksum += proctab_[task].mpirank + i * nEqClasses + 1;
            }
        }
        edgeAttr.label = (void *)countRepEdge;
    }
    else
    {
        edge = initializeBitVectorEdge(proctabSize_);
        if (edge == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to initialize edge\n");
            return NULL;
        }
        for (i = 0; i < nTasks / nEqClasses; i++)
        {
            currentTask = task + i * nEqClasses;
            edge->bitVector[currentTask / STAT_BITVECTOR_BITS] |= STAT_GRAPH_BIT(currentTask % STAT_BITVECTOR_BITS);
        }
        if (nTasks % nEqClasses > task)
        {
            currentTask = task + i * nEqClasses;
            edge->bitVector[currentTask / STAT_BITVECTOR_BITS] |= STAT_GRAPH_BIT(currentTask % STAT_BITVECTOR_BITS);
        }
        edgeAttr.label = (void *)edge;
    }

    /* See if we've already generated a graph for this equivalence class and iteration */
    if (task == 0 && nEqClasses != -1)
    {
        for (i = 0; i < generated_graphs.size(); i++)
        {
            /* Free up the graph previously cached graphs */
            graphlibError = graphlib_delGraph(generated_graphs[i]);
            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
                return NULL;
            }
        }
        generated_graphs.clear();
    }

    /* create return graph */
    /* Create graph root */
    if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
       retGraph = createRootedGraph(statCountRepFunctions);
    else
       retGraph = createRootedGraph(statBitVectorFunctions);
    if (retGraph == NULL)
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to create new graph\n");
        return NULL;
    }

    path = "/";
    prevId = 0;

    /* Create artificial libc_start_main */
    fillInName("__libc_start_main", nodeAttr);
    path += std::string((char *) nodeAttr.label);
    nodeId = statStringHash(path.c_str());
    graphlibError = graphlib_addNode(retGraph, nodeId, &nodeAttr);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add node\n");
        return NULL;
    }
    graphlibError = graphlib_addDirectedEdge(retGraph, prevId, nodeId, &edgeAttr);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add edge\n");
        return NULL;
    }
    prevId = nodeId;

    /* Create artificial main */
    fillInName("main", nodeAttr);
    path += std::string((char *) nodeAttr.label);

    nodeId = statStringHash(path.c_str());
    graphlibError = graphlib_addNode(retGraph, nodeId, &nodeAttr);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add node\n");
        return NULL;
    }
    graphlibError = graphlib_addDirectedEdge(retGraph, prevId, nodeId, &edgeAttr);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add edge\n");
        return NULL;
    }
    prevId = nodeId;

    /* Seed the random number generator based on my equivalence class */
    if (nEqClasses == -1) /* no limit */
        srand(task + 999999 * iteration);
    else
        srand(task % nEqClasses +  999999 * (1 + iteration));

    /* Generate a atack trace and add it to the graph */
    depth = rand() % maxDepth;
    for (i = 0; i < depth; i++)
    {
        snprintf(temp, 8192, "depth%dfun%d", i, rand() % functionFanout);
        fillInName(temp, nodeAttr);
        path += string((char *) nodeAttr.label);
        nodeId = statStringHash(path.c_str());

        graphlibError = graphlib_addNode(retGraph, nodeId, &nodeAttr);
        if (GRL_IS_FATALERROR(graphlibError))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add node\n");
            return NULL;
        }
        graphlibError = graphlib_addDirectedEdge(retGraph, prevId, nodeId, &edgeAttr);
        if (GRL_IS_FATALERROR(graphlibError))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add edge\n");
            return NULL;
        }
        prevId = nodeId;
    }
    if (sampleType_ == STAT_CR_FUNCTION_NAME_ONLY || sampleType_ == STAT_CR_FUNCTION_AND_PC || sampleType_ == STAT_CR_FUNCTION_AND_LINE)
        graphlibError = graphlib_delEdgeAttr(edgeAttr, statFreeCountRepEdge);
    else
        graphlibError = graphlib_delEdgeAttr(edgeAttr, statFreeEdge);
    if (GRL_IS_FATALERROR(graphlibError))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to free edge attr\n");
        return NULL;
    }

    if (nEqClasses != -1)
        generated_graphs.push_back(retGraph);
    return retGraph;
}


