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
#include <set>

using namespace std;
using namespace MRN;

using namespace Dyninst;
using namespace Dyninst::Stackwalker;
using namespace Dyninst::SymtabAPI;
#ifdef GROUP_OPS
    using namespace Dyninst::ProcControlAPI;
#endif


#ifdef STAT_FGFS
    using namespace FastGlobalFileStat;
    using namespace FastGlobalFileStat::MountPointAttribute;
    using namespace FastGlobalFileStat::CommLayer;
#endif

#ifdef GRL_DYNAMIC_NODE_NAME
#define GRAPH_FONT_SIZE 1
#else
#define GRAPH_FONT_SIZE -1
#endif
StatError_t statInit(int *argc, char ***argv)
{
    lmon_rc_e rc;

    /* Call LaunchMON's Init function */
    rc = LMON_be_init(LMON_VERSION, argc, argv);
    if (rc != LMON_OK)
    {
        fprintf(stderr, "Failed to initialize Launchmon\n");
        return STAT_LMON_ERROR;
    }

    return STAT_OK;
}

StatError_t statFinalize()
{
    lmon_rc_e rc;

    /* Call LaunchMON's Finalize function */
    rc = LMON_be_finalize();
    if (rc != LMON_OK)
    {
        fprintf(stderr, "Failed to finalize Launchmon\n");
        return STAT_LMON_ERROR;
    }

    return STAT_OK;
}

STAT_BackEnd::STAT_BackEnd()
{
    char abuf[INET_ADDRSTRLEN], fileName[BUFSIZE], *envValue; 
    FILE *f;
    struct sockaddr_in addr, *sinp = NULL;
    struct addrinfo *addinf = NULL;

    /* get hostname and IP address */
    gethostname(localHostName_, BUFSIZE);
    getaddrinfo(localHostName_, NULL, NULL, &addinf);
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
        f = fopen(fileName, "w");
        Dyninst::Stackwalker::setDebug(true);
        Dyninst::Stackwalker::setDebugChannel(f); 
#ifdef GROUP_OPS
        Dyninst::ProcControlAPI::setDebug(true);
        Dyninst::ProcControlAPI::setDebugChannel(f);
#endif
    }

    processMapNonNull_ = 0;
    statOutFp = NULL;
    initialized_ = false;
    connected_ = false;
    isRunning_ = false;
    network_ = NULL;
    proctab_ = NULL;
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
    graphlib_error_t gl_err;

    /* delete exisitng graphs */
    if (prefixTree3d_ != NULL)
    {
        gl_err = graphlib_delGraph(prefixTree3d_);
        if (GRL_IS_FATALERROR(gl_err))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
    }
    if (prefixTree2d_ != NULL)
    {
        gl_err = graphlib_delGraph(prefixTree2d_);
        if (GRL_IS_FATALERROR(gl_err))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
    }

    /* free the parent hostname */
    if (parentHostName_ != NULL)
        free(parentHostName_);

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
    }

#ifdef STAT_FGFS
    if (fgfsCommFabric_ != NULL)
        delete fgfsCommFabric_;
#endif
    /* clean up MRNet */
    if (network_ != NULL)
    {
#ifdef MRNET3
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "waiting for shutdown\n");
        network_->waitfor_ShutDown();
#endif
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "deleting network\n");
        delete network_;
    }
}

StatError_t STAT_BackEnd::Init()
{
    int tempSize;
    lmon_rc_e rc;

    /* Register unpack function with Launchmon */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Registering unpack function with Launchmon\n");
    rc = LMON_be_regUnpackForFeToBe(Unpack_STAT_BE_info);
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to register unpack function\n");
        return STAT_LMON_ERROR;
    }

    /* Launchmon handshake */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Launchmon handshaking\n");
    rc = LMON_be_handshake(NULL);
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to handshake with Launchmon\n");
        return STAT_LMON_ERROR;
    }

    /* Wait for Launchmon to be ready*/
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Waiting for launchmon to be ready\n");
    rc = LMON_be_ready(NULL);
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Launchmon failed to get ready\n");
        return STAT_LMON_ERROR;
    }

    /* Allocate my process table */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Allocating process table\n");
    rc = LMON_be_getMyProctabSize(&tempSize);
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to get Process Table Size\n");
        return STAT_LMON_ERROR;
    }
    proctab_ = (MPIR_PROCDESC_EXT *)malloc(tempSize * sizeof(MPIR_PROCDESC_EXT));
    if (proctab_ == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Error allocating process table\n");
        return STAT_ALLOCATE_ERROR;
    }

    /* Get my process table entries */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Getting my process table entries\n");
    rc = LMON_be_getMyProctab(proctab_, &proctabSize_, tempSize);
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to get Process Table\n");
        return STAT_LMON_ERROR;
    }

    initialized_ = true;
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Launchmon successfully initialized\n");

    return STAT_OK;
}

StatError_t STAT_BackEnd::Connect()
{
    int i;
    bool found;
    string prettyHost, leafPrettyHost;
    lmon_rc_e rc;
    statLeafInfoArray_t leafInfoArray;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Prepearing to connect to MRNet\n");

    /* Make sure we've been initialized through Launchmon */
    if (initialized_ == false)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Trying to connect when not initialized\n");
        return STAT_LMON_ERROR;
    }

    /* Receive MRNet connection information */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Receiving connection information from FE\n");
    rc = LMON_be_recvUsrData((void *)&leafInfoArray); 
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to receive data from FE\n");
        return STAT_LMON_ERROR;
    }

    /* Master broadcast number of daemons */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Broadcasting size of connection info to daemons\n");
    rc = LMON_be_broadcast((void *)&(leafInfoArray.size), sizeof(int));
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to broadcast num_leaves\n");
        return STAT_LMON_ERROR;
    }

    /* Non-masters allocate space for the MRNet connection info */
    if (LMON_be_amIMaster() == LMON_NO)
    {
        leafInfoArray.leaves = (statLeafInfo_t *)malloc(leafInfoArray.size * sizeof(statLeafInfo_t));
        if (leafInfoArray.leaves == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to allocate %d x %d = %dB of memory for leaf info\n", leafInfoArray.size, sizeof(statLeafInfo_t), leafInfoArray.size*sizeof(statLeafInfo_t));
            return STAT_ALLOCATE_ERROR;
        }
    }

    /* Master broadcast MRNet connection information to all daemons */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Broadcasting connection information to all daemons\n");
    rc = LMON_be_broadcast((void *)(leafInfoArray.leaves), leafInfoArray.size * sizeof(statLeafInfo_t));
    if (rc != LMON_OK)
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
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed on call to strdup()\n");
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
#ifdef MRNET22
    char *param[6], parentPort[BUFSIZE], parentRank[BUFSIZE], myRank[BUFSIZE];
    snprintf(parentPort, BUFSIZE, "%d", parentPort_);
    snprintf(parentRank, BUFSIZE, "%d", parentRank_);
    snprintf(myRank, BUFSIZE, "%d", myRank_);
    param[0] = NULL;
    param[1] = parentHostName_;
    param[2] = parentPort;
    param[3] = parentRank;
    param[4] = localHostName_;
    param[5] = myRank;
    printMsg(STAT_LOG_MESSAGE,__FILE__,__LINE__,"Calling CreateNetworkBE\n"); 
    network_ = Network::CreateNetworkBE(6, param);
    printMsg(STAT_LOG_MESSAGE,__FILE__,__LINE__,"End of create network be\n");
#else
    network_ = new Network(parentHostName_, parentPort_, parentRank_, localHostName_, myRank_);
#endif
    if(network_ == NULL)
    {
        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "backend_init() failed\n");
        return STAT_MRNET_ERROR;
    }

    /* Free the Leaf Info array */
    if (leafInfoArray.leaves != NULL)
        free(leafInfoArray.leaves);

    connected_ = true;
    return STAT_OK;
}

StatError_t STAT_BackEnd::mainLoop()
{
    int tag = 0, retval, nEqClasses, swNotificationFd, mrnNotificationFd, max_fd;
    unsigned int nTraces, traceFrequency, nTasks, functionFanout, maxDepth, nRetries, retryFrequency, withThreads, clearOnSample;
#ifdef GRAPHLIB16
    unsigned long obyteArrayLen;
#else
    unsigned int obyteArrayLen;
#endif
    char *obyteArray = NULL, *variableSpecification;
    Stream *stream;
    fd_set readfds, writefds, exceptfds;
    PacketPtr packet;
    StatError_t statError;
    graphlib_error_t gl_err;

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
#ifdef MRNET3            
           mrnNotificationFd = network_->get_EventNotificationFd(MRN::Event::DATA_EVENT);
#else            
            mrnNotificationFd = network_->get_EventNotificationFd(DATA_EVENT);
#endif            
            /* setup the select call for the MRNet and SW FDs */
            if (mrnNotificationFd > swNotificationFd)
                max_fd = mrnNotificationFd + 1;
            else
                max_fd = swNotificationFd + 1;
            FD_ZERO(&readfds);
            FD_ZERO(&writefds);
            FD_ZERO(&exceptfds);
            FD_SET(mrnNotificationFd, &readfds);
            FD_SET(swNotificationFd, &readfds);

            /* call select to get pending requests */
            retval = select(max_fd, &readfds, &writefds, &exceptfds, NULL);
            if (retval < 0 && errno != EINTR)
            {
                printMsg(STAT_SYSTEM_ERROR, __FILE__, __LINE__, "%s: select failed\n", strerror(errno));
                //TODO: do we really want to exit?
                return STAT_SYSTEM_ERROR;
            }
            if (FD_ISSET(mrnNotificationFd, &readfds))
            {
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Handling MRNet event on FD %d\n", mrnNotificationFd);
#ifdef MRNET3
                network_->clear_EventNotificationFd(MRN::Event::DATA_EVENT);
#else                
                network_->clear_EventNotificationFd(DATA_EVENT);
#endif                
            }
            else if (FD_ISSET(swNotificationFd, &readfds))
            {
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Handling debug event on FD %d\n", swNotificationFd);
                bool result;
                map<int, Walker *>::iterator iter;
                result = ProcDebug::handleDebugEvent(false);
                if (result == false)
                {
                   printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Error handling debug events: %s\n", Stackwalker::getLastErrorMsg());
                    //TODO: do we really want to exit?
                    //return STAT_STACKWALKER_ERROR;
                }
                
                /* Check is target processes exited, if so, set Walker to NULL */
                for (iter = processMap_.begin(); iter != processMap_.end(); iter++)
                {
                    if (iter->second == NULL)
                        continue;
                    ProcDebug *pdebug = dynamic_cast<ProcDebug *>(iter->second->getProcessState());
                    if (pdebug)
                    {
                        if (pdebug->isTerminated())
                        {
                            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Task %d has exited\n", iter->first);
                            delete iter->second;
                            processMapNonNull_ = processMapNonNull_ - 1;
                            iter->second = NULL;
                            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Process map now contains %d processes, non-null count = %d\n", processMap_.size(), processMapNonNull_);
                        }
                    }
                }
                continue;
            }
        }
        /* Receive the packet from the STAT FE */
        /* Use non-blocking recv to catch any false positive events */
        retval = network_->recv(&tag, packet, &stream, false);
        if (retval == 0)
            continue;
        else if (retval != 1)
        {
            printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::recv() failure\n");
            return STAT_MRNET_ERROR;
        }
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Packet received with tag %d\n", tag);

        /* Initialize return value to error code */
        retval = 1;

        switch(tag)
        {
            case PROT_ATTACH_APPLICATION:
                /* Attach to the application */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to attach\n");
                statError = Attach();
                if (statError == STAT_OK)
                    retval = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Attach complete, sending ackowledgement\n");
                if (sendAck(stream, PROT_ATTACH_APPLICATION_RESP, retval) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_ATTACH_APPLICATION_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }
                break;

            case PROT_PAUSE_APPLICATION:
                /* Pause to the application */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to pause\n");
                statError = Pause();
                if (statError == STAT_OK)
                    retval = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Pause complete, sending ackowledgement\n");
                if (sendAck(stream, PROT_PAUSE_APPLICATION_RESP, retval) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_PAUSE_APPLICATION_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }
                break;

            case PROT_RESUME_APPLICATION:
                /* Resume to the application */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to resume\n");
                statError = Resume();
                if (statError == STAT_OK)
                    retval = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Resume complete, sending ackowledgement\n");
                if (sendAck(stream, PROT_RESUME_APPLICATION_RESP, retval) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_RESUME_APPLICATION_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }
                break;

            case PROT_SAMPLE_TRACES:
                /* Unpack the message */
                if (packet->unpack("%ud %ud %ud %ud %ud %ud %ud %s", &nTraces, &traceFrequency, &nRetries, &retryFrequency, &sampleType, &withThreads, &clearOnSample, &variableSpecification) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_SAMPLE_TRACES) failed\n");
                    if (sendAck(stream, PROT_SAMPLE_TRACES_RESP, retval) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_SAMPLE_TRACES_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }
                    return STAT_MRNET_ERROR;
                }
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to sample %d traces each %d us with %d retries every %d us\n", nTraces, traceFrequency, nRetries, retryFrequency);

                /* Gather stack traces */
                statError = sampleStackTraces(nTraces, traceFrequency, nRetries, retryFrequency, withThreads, clearOnSample, variableSpecification);
                if (statError == STAT_OK)
                    retval = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Sampling complete, sending ackowledgement\n");
                if (sendAck(stream, PROT_SAMPLE_TRACES_RESP, retval) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_SAMPLE_TRACES_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }

                break;

            case PROT_SEND_LAST_TRACE:
                /* Send merged traces to the FE */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to send traces to the FE\n");

                /* Free previously allocated byte arrays */
                if (obyteArray != NULL)
                {
                    free(obyteArray);
                    obyteArray = NULL;
                }

                /* Serialize 2D graph */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Serializing 2D graph\n");
                gl_err = graphlib_serializeGraph(prefixTree2d_, &obyteArray, &obyteArrayLen);
                if (GRL_IS_FATALERROR(gl_err))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to serialize 3D prefix tree\n");
                    return STAT_GRAPHLIB_ERROR;
                }

                /* Send */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Sending graphs to FE\n");
#ifdef MRNET40
                if (stream->send(PROT_SEND_LAST_TRACE_RESP, "%Ac %d %d", obyteArray, obyteArrayLen, proctabSize_, myRank_) == -1)
#else
                if (stream->send(PROT_SEND_LAST_TRACE_RESP, "%ac %d %d", obyteArray, obyteArrayLen, proctabSize_, myRank_) == -1)
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

                /* Free previously allocated byte arrays */
                if (obyteArray != NULL)
                {
                    free(obyteArray);
                    obyteArray = NULL;
                }

                /* Serialize 3D graph */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Serializing 3D graph\n");
                gl_err = graphlib_serializeGraph(prefixTree3d_, &obyteArray, &obyteArrayLen);
                if (GRL_IS_FATALERROR(gl_err))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to serialize 3D prefix tree\n");
                    return STAT_GRAPHLIB_ERROR;
                }

                /* Send */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Sending graphs to FE\n");
#ifdef MRNET40
                if (stream->send(PROT_SEND_TRACES_RESP, "%Ac %d %d", obyteArray, obyteArrayLen, proctabSize_, myRank_) == -1)
#else
                if (stream->send(PROT_SEND_TRACES_RESP, "%ac %d %d", obyteArray, obyteArrayLen, proctabSize_, myRank_) == -1)
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
                unsigned int *stopArray;
                int stopArrayLen;
                if (packet->unpack("%aud", &stopArray, &stopArrayLen) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_DETACH_APPLICATION) failed\n");
                    if (sendAck(stream, PROT_DETACH_APPLICATION_RESP, retval) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_DETACH_APPLICATION_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }
                    return STAT_MRNET_ERROR;
                }

                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Detaching, leaving %d processes stopped\n", stopArrayLen);
                statError = Detach(stopArray, stopArrayLen);
                if (statError ==  STAT_OK)
                    retval = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Detaching complete, sending ackowledgement\n");
                if (sendAck(stream, PROT_DETACH_APPLICATION_RESP, retval) != STAT_OK)
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
                    retval = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Termination complete, sending ackowledgement\n");
                if (sendAck(stream, PROT_TERMINATE_APPLICATION_RESP, retval) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_TERMINATE_APPLICATION_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }

                break;

            case PROT_STATBENCH_CREATE_TRACES:
                /* Create stack traces */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to create traces\n");

                /* Unpack message */
                if (packet->unpack("%ud %ud %ud %ud %d", &maxDepth, &nTasks, &nTraces, &functionFanout, &nEqClasses) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_STATBENCH_CREATE_TRACES) failed\n");
                    if (sendAck(stream, PROT_STATBENCH_CREATE_TRACES_RESP, retval) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_STATBENCH_CREATE_TRACES_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }
                    return STAT_MRNET_ERROR;
                }
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to create traces with max depth = %d, num tasks = %d, num traces = %d, function fanout = %d, equivalence classes = %d\n", maxDepth, nTasks, nTraces, functionFanout, nEqClasses);

                /* Create stack traces */
                statError = statBenchCreateTraces(maxDepth, nTasks, nTraces, functionFanout, nEqClasses);
                if (statError == STAT_OK)
                    retval = 0;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Finished creating traces, sending ackowledgement\n");
                if (sendAck(stream, PROT_STATBENCH_CREATE_TRACES_RESP, retval) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_STATBENCH_CREATE_TRACES_RESP) failed\n");
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
                    if (sendAck(stream, PROT_CHECK_VERSION_RESP, retval) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_CHECK_VERSION_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }
                    return STAT_MRNET_ERROR;
                }
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to compare my version %d.%d.%d to the FE version %d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION, major, minor, revision);

                /* Compare version numbers */
                if (major == STAT_MAJOR_VERSION && minor == STAT_MINOR_VERSION && revision == STAT_REVISION_VERSION)
                    retval = 0;

                /* Send ack packet */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Finished comparing version, sending ackowledgement\n");
                if (stream->send(PROT_CHECK_VERSION_RESP, "%d %d %d %d %d", major, minor, revision, retval, 0) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::send() failure\n");
                    return STAT_MRNET_ERROR;
                }
                if (stream->flush() == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::flush() failure\n");
                    return STAT_MRNET_ERROR;
                }

                if (retval != 0)
                    printMsg(STAT_VERSION_ERROR, __FILE__, __LINE__, "Daemon reports version mismatch: FE = %d.%d.%d, Daemon = %d.%d.%d\n", major, minor, revision, STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);

                break;

            case PROT_SEND_BROADCAST_STREAM:
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received broadcast stream\n");
                broadcastStream_ = stream;

                /* Send ack */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Finished creating traces, sending ackowledgement\n");
                if (sendAck(stream, PROT_SEND_BROADCAST_STREAM_RESP, 0) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_STATBENCH_CREATE_TRACES_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }
                break;

#ifdef STAT_FGFS
            case PROT_SEND_FGFS_STREAM:
                bool ret;

                /* Initialize the FGFS Comm Fabric */
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received message to setup FGFS\n");
                ret = MRNetCommFabric::initialize((void *)network_, (void *)stream);
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

                break;

            case PROT_FILE_REQ:
                MRNetSymbolReaderFactory *msrf;
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to register file request stream \n");
        
                //Save the Stream for sending File requests later
                msrf = new MRNetSymbolReaderFactory(Walker::getSymbolReader(), network_, stream);
                Walker::setSymbolReader(msrf);            
                retval = 0;

                //Send File Request Acknowledgement
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Finished registering file request stream, sending ackowledgement\n");
                if (sendAck(broadcastStream_, PROT_FILE_REQ_RESP, retval) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_FILE_REQ_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }
                break;

#endif //FGFS

            case PROT_EXIT:
                /* Exit */
                break;

            default:
                /* Unknown tag */
                printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Unknown Protocol: %d\n", tag);
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

StatError_t STAT_BackEnd::Attach()
{
    int i;
    StatError_t ret;
    Walker *proc;

    /* Attach to the processes in the process table */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Attaching to all application processes\n");

#if defined(GROUP_OPS)
    vector<ProcessSet::AttachInfo> ainfo;
    if (doGroupOps_) {
       ainfo.reserve(proctabSize_);
       for (i = 0; i < proctabSize_; i++)
       {
          printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Group attach includes proces %s, pid %d, MPI rank %d\n", proctab_[i].pd.executable_name, proctab_[i].pd.pid, proctab_[i].mpirank);
          ProcessSet::AttachInfo pattach;
          pattach.pid = proctab_[i].pd.pid;
          pattach.executable = proctab_[i].pd.executable_name;
          pattach.error_ret = ProcControlAPI::err_none;
          ainfo.push_back(pattach);
       }
       procset = ProcessSet::attachProcessSet(ainfo);
       walkerset = WalkerSet::newWalkerSet();
    }
#endif

    /* Attach to the processes in the process table */
    for (i = 0; i < proctabSize_; i++)
    {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Attaching to process %s, pid %d, MPI rank %d\n", proctab_[i].pd.executable_name, proctab_[i].pd.pid, proctab_[i].mpirank);

        Walker *proc;
#if defined(GROUP_OPS)
        if (doGroupOps_) {
           Process::ptr pc_proc = ainfo[i].proc;
           proc = pc_proc ? Walker::newWalker(pc_proc) : NULL;
           if (proc == NULL)
           {
              printMsg(STAT_WARNING, __FILE__, __LINE__, "Group Attach to task rank %d, pid %d failed with message '%s'\n", proctab_[i].mpirank, proctab_[i].pd.pid, Dyninst::ProcControlAPI::getGenericErrorMsg(ainfo[i].error_ret));
           }
           else {
              pc_proc->setData(proc); //Up ptr for mapping Process::ptr -> Walker
              walkerset->insert(proc);
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

        assert(proc);
        processMap_[proctab_[i].mpirank] = proc;
        if (proc != NULL) {
            processMapNonNull_++;
        }
    }
    map<int, Walker *>::iterator j;
    for (i=0, j = processMap_.begin(); j != processMap_.end(); i++, j++) {
       procsToRanks_.insert(make_pair(j->second, i));
    }
    isRunning_ = false;

    return STAT_OK;
}

StatError_t STAT_BackEnd::Pause()
{
    /* Pause all processes */
   printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Pausing all application processes\n");
#if defined(GROUP_OPS)
   if (doGroupOps_) {
      procset->stopProcs();
   }
   else
#endif
   {
      map<int, Walker *>::iterator iter;
      for (iter = processMap_.begin(); iter != processMap_.end(); iter++)
      {
         pauseImpl(iter->second);
      }
   }
    isRunning_ = false;

    return STAT_OK;
}

StatError_t STAT_BackEnd::Resume()
{
    /* Resume all processes */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Resuming all application processes\n");
#if defined(GROUP_OPS)
    if (doGroupOps_) {
       procset->continueProcs();
    }
    else
#endif
    {
       map<int, Walker *>::iterator iter;
       for (iter = processMap_.begin(); iter != processMap_.end(); iter++)
       {
          resumeImpl(iter->second);
       }
    }
    isRunning_ = true;

    return STAT_OK;
}

StatError_t STAT_BackEnd::pauseImpl(Walker *walker)
{
    /* Pause the process */
    bool ret;
    if (walker != NULL)
    {
        ProcDebug *pdebug = dynamic_cast<ProcDebug *>(walker->getProcessState());
        if (pdebug == NULL)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to dynamic_cast ProcDebug pointer\n");
            return STAT_STACKWALKER_ERROR;
        }
        if (pdebug->isTerminated())
            return STAT_OK;
        ret = pdebug->pause();
        if (ret == false)
            printMsg(STAT_WARNING, __FILE__, __LINE__, "Failed to pause process\n");
    }
    return STAT_OK;
}

StatError_t STAT_BackEnd::resumeImpl(Walker *walker)
{
    /* Resume the process */
    bool ret;
    if (walker != NULL)
    {
        ProcDebug *pdebug = dynamic_cast<ProcDebug *>(walker->getProcessState());
        if (pdebug == NULL)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to dynamic_cast ProcDebug pointer\n");
            return STAT_STACKWALKER_ERROR;
        }
        if (pdebug->isTerminated())
            return STAT_OK;
        ret = pdebug->resume();
        if (ret == false)
            printMsg(STAT_WARNING, __FILE__, __LINE__, "Failed to resume process\n");
    }
    return STAT_OK;
}

#ifdef SBRS
int bcastWrapper(void *buf, int size)
{
    LMON_be_broadcast(buf, size);
    return 0;
}

int isMasterWrapper()
{
    if (LMON_be_amIMaster() == LMON_YES)
        return 1;
    return 0;
}
#endif

StatError_t STAT_BackEnd::parseVariableSpecification(char *variableSpecification, statVariable_t **outBuf, int *nVariables)
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
    *nVariables = nElements;
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "%d elements in variable specification\n", nElements);
    (*outBuf) = (statVariable_t *)malloc(nElements * sizeof(statVariable_t));
    if ((*outBuf) == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to allocate %d elements\n", nElements);
    }
    cur = spec.substr(delimPos + 1, spec.length());

    /* Get each element */
    for (i = 0; i < nElements; i++)
    {
        /* Get the file name */
        delimPos = cur.find_first_of(":");
        temp = cur.substr(0, delimPos);
        snprintf((*outBuf)[i].fileName, BUFSIZE, "%s", temp.c_str());
        cur = cur.substr(delimPos + 1, cur.length());

        /* Get the line number */
        delimPos = cur.find_first_of(".");
        temp = cur.substr(0, delimPos);
        (*outBuf)[i].lineNum = atoi(temp.c_str());
        cur = cur.substr(delimPos + 1, cur.length());

        /* Get the stack depth */
        delimPos = cur.find_first_of("$");
        temp = cur.substr(0, delimPos);
        (*outBuf)[i].depth = atoi(temp.c_str());
        cur = cur.substr(delimPos + 1, cur.length());

        /* Get the variable name */
        if (i == nElements - 1)
            delimPos = cur.length();
        else
            delimPos = cur.find_first_of(",");
        temp = cur.substr(0, delimPos);
        snprintf((*outBuf)[i].variableName, BUFSIZE, "%s", temp.c_str());
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "elements: %s %d %d %s\n", (*outBuf)[i].fileName, (*outBuf)[i].lineNum, (*outBuf)[i].depth, (*outBuf)[i].variableName);        
        if (i != nElements - 1)
            cur = cur.substr(delimPos + 1, cur.length());
    }

    return STAT_OK;
}

StatError_t STAT_BackEnd::getSourceLine(Walker *proc, Address addr, char *outBuf, int *lineNum)
{
    bool ret;
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
    if (!libState)
    {
        printMsg(STAT_WARNING, __FILE__, __LINE__, "Failed to get LibraryState\n");
        snprintf(outBuf, BUFSIZE, "?");
        return STAT_OK;
    }
    ret = libState->getLibraryAtAddr(addr, lib);
    if (!ret)
    {
        printMsg(STAT_WARNING, __FILE__, __LINE__, "Failed to get library at address 0x%lx\n", addr);
        snprintf(outBuf, BUFSIZE, "?");
        return STAT_OK;
    }
    ret = Symtab::openFile(symtab, lib.first);
    if (ret == false || symtab == NULL)
    {
        printMsg(STAT_WARNING, __FILE__, __LINE__, "Symtab failed to open file %s\n", lib.first.c_str());
        snprintf(outBuf, BUFSIZE, "?");
        return STAT_OK;
    }
    symtab->setTruncateLinePaths(false);
    loadAddr = lib.second;
    ret = symtab->getSourceLines(lines, addr - loadAddr);
    if (!ret)
    {
        snprintf(outBuf, BUFSIZE, "?");
        return STAT_OK;
    }

    snprintf(outBuf, BUFSIZE, "%s", lines[0]->getFile().c_str());
    *lineNum = lines[0]->getLine();
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "%lx resolved to %s:%d\n", addr, outBuf, *lineNum);

    return STAT_OK;
}

StatError_t STAT_BackEnd::getVariable(vector<Frame> swalk, unsigned int frame, char *variableName, char *outBuf)
{
#ifdef PROTOTYPE
    int result;
    bool ret;
    char buf[BUFSIZE];
    Function *func = NULL;
    localVar *var;
    vector<localVar *> vars;
    vector<localVar *>::iterator iter;
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Searching for variable %s\n", variableName);

    func = getFunctionForFrame(swalk[frame]);
    if (func == NULL)
    {
        printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to get function for this frame\n");
        return STAT_STACKWALKER_ERROR;
    }

    ret = func->getLocalVariables(vars);
    if (ret == false)
    {
        printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to get local variables for this function\n");
        return STAT_STACKWALKER_ERROR;
    }

    for (iter = vars.begin(); iter != vars.end(); iter++)
    {
        var = *iter;
        if (strcmp(var->getName().c_str(), variableName) == 0)
        {
            result = getLocalVariableValue(*iter, swalk, frame, buf, BUFSIZE);
            if (result != glvv_Success)
            {
                printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to get variable %s for this function\n", variableName);
                return STAT_STACKWALKER_ERROR;
            }
            if (var->getType()->getSize() == 8)
                snprintf(outBuf, BUFSIZE, "$%s=%d", variableName, *((int *)buf));
            else if (var->getType()->getSize() == 4)
                snprintf(outBuf, BUFSIZE, "$%s=%d", variableName, *((int *)buf));
        }
    }
 
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Found variable %s\n", outBuf);
#else    
    printMsg(STAT_WARNING, __FILE__, __LINE__, "Prototype variable extraction feature not enabled!\n");
    snprintf(outBuf, BUFSIZE, "$%s=-1", variableName);
#endif    
    return STAT_OK;
}

StatError_t STAT_BackEnd::mergeIntoGraphs(graphlib_graph_p currentGraph, bool last_trace)
{
   graphlib_error_t gl_err;
   /* Merge cur graph into retGraph */
   gl_err = graphlib_mergeGraphsRanked(prefixTree3d_, currentGraph);
   if (GRL_IS_FATALERROR(gl_err))
   {
      printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error merging graphs\n");
      return STAT_GRAPHLIB_ERROR;
   }
   if (last_trace)
   {
      gl_err = graphlib_mergeGraphsRanked(prefixTree2d_, currentGraph);
      if (GRL_IS_FATALERROR(gl_err))
      {
         printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error merging graphs\n");
         return STAT_GRAPHLIB_ERROR;
      }
   }
   
   /* Free up current graph */
   gl_err = graphlib_delGraph(currentGraph);
   if (GRL_IS_FATALERROR(gl_err))
   {
      printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
      return STAT_GRAPHLIB_ERROR;
   } 
   return STAT_OK;
}

graphlib_graph_p STAT_BackEnd::createRootedGraph()
{
   graphlib_error_t gl_err;
   graphlib_graph_p newGraph = NULL;
   gl_err = graphlib_newGraph(&newGraph);
   if (GRL_IS_FATALERROR(gl_err))
   {
      printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error creating new graph\n");
      return NULL;
   }

   graphlib_nodeattr_t nodeattr = {1,0,20,GRC_LIGHTGREY,0,0,(char *) "/", GRAPH_FONT_SIZE};
   gl_err = graphlib_addNode(newGraph, 0, &nodeattr);
   if (GRL_IS_FATALERROR(gl_err)) {
      printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error adding sentinel node to graph\n");
      return NULL;
   }
   return newGraph;
}

StatError_t STAT_BackEnd::sampleStackTraces(unsigned int nTraces, unsigned int traceFrequency, unsigned int nRetries, unsigned int retryFrequency, unsigned int withThreads, unsigned int clearOnSample, char *variableSpecification)
{
    int j, retval;
    unsigned int i;
    bool wasRunning = isRunning_;
    graphlib_graph_p currentGraph = NULL;
    graphlib_error_t gl_err;
    StatError_t ret;
    map<int, Walker *>::iterator iter;
    
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Preparing to sample stack traces\n");

#ifdef SBRS
    char **libraryList;
    int nlib;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Broadcasting target process files\n");

    /* Pause process to yield CPU */
    if (isRunning_)
    {
        ret = Pause();
        if (ret != STAT_OK)
           printMsg(ret, __FILE__, __LINE__, "Failed to pause process\n");
    }

    /* Broadcast file contents */
    sbrs_bind_bcast(bcastWrapper);
    sbrs_bind_is_master(isMasterWrapper);
    libraryList = sbrs_fet_liblist(proctab_[0].pd.executable_name, &nlib);
    sbrs_bcast_binary_list("/tmp", proctab_[0].pd.executable_name, libraryList, nlib);
    sbrs_enable_interp();

    /* Set process back to running */
    if (wasRunning)
    {
       ret = Resume();
       if (ret != STAT_OK)
          printMsg(ret, __FILE__, __LINE__, "Failed to pause process\n");
    }
#endif

    /* Initialize graphlib if we haven't already done so */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Initializing graphlib with process table size = %d\n", proctabSize_);
    gl_err = graphlib_InitVarEdgeLabels(proctabSize_);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to initialize graphlib\n");
        return STAT_GRAPHLIB_ERROR;
    }

    /* Delete previously merged graphs */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Deleting any previously mereged trees\n");
    if (prefixTree3d_ != NULL && clearOnSample == 1)
    {
        gl_err = graphlib_delGraph(prefixTree3d_);
        if (GRL_IS_FATALERROR(gl_err))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
        prefixTree3d_ = NULL;
    }
    if (prefixTree2d_ != NULL)
    {
        gl_err = graphlib_delGraph(prefixTree2d_);
        if (GRL_IS_FATALERROR(gl_err))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
    }

    /* Create graphs */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Creating graphs\n");
    if (prefixTree3d_ == NULL)
    {
        gl_err = graphlib_newGraph(&prefixTree3d_);
        if (GRL_IS_FATALERROR(gl_err))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error initializing 3D graph\n");
            return STAT_GRAPHLIB_ERROR;
        }
    }
    gl_err = graphlib_newGraph(&prefixTree2d_);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error initializing 2D graph\n");
        return STAT_GRAPHLIB_ERROR;
    }

    /* Parse the variable specification */
    nVariables = 0;
    if (strcmp(variableSpecification, "NULL") != 0)
    {
        ret = parseVariableSpecification(variableSpecification, &extractVariables, &nVariables);
        if (ret != STAT_OK)
        {
            printMsg(ret, __FILE__, __LINE__, "Failed to parse variable specification\n");
            return ret;
        }
    }

    /* Gather stack traces and merge */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Gathering and merging %d traces from each task\n", nTraces);
    for (i = 0; i < nTraces; i++)
    {
        bool last_trace = (i == nTraces-1);
        /* Pause process as necessary */
        if (isRunning_) {
           ret = Pause();
           if (ret != STAT_OK)
              printMsg(ret, __FILE__, __LINE__, "Failed to pause processes\n");
        }

        /* Collect call stacks from each process */
#if defined(GROUP_OPS)
        if (doGroupOps_) {
           /* Create return graph */
           currentGraph = createRootedGraph();
        
           /* Get the stack trace */
           getStackTraceFromAll(currentGraph, nRetries, retryFrequency, withThreads);
           
           /* Merge into prefixTree2d and prefixTree3d */
           ret = mergeIntoGraphs(currentGraph, last_trace);
           if (ret != STAT_OK)
              return ret;


           gl_err = graphlib_exportGraph((char *) "mergegraph.dot", GRF_DOT, prefixTree3d_);
           if (GRL_IS_FATALERROR(gl_err))
              fprintf(stderr, "Error exporting graph\n");
        }
        else
#endif
        {
           int j;
           for (iter = processMap_.begin(), j=0; iter != processMap_.end(); iter++, j++)
           {
              /* Create return graph */
              currentGraph = createRootedGraph();
              
              /* Get the stack trace */
              ret = getStackTrace(currentGraph, iter->second, j, nRetries, retryFrequency, 
                                  withThreads);
              if (ret != STAT_OK)
              {
                 printMsg(ret, __FILE__, __LINE__, "Error getting graph %d of %d\n", i+1, nTraces);
                 return ret;
              }
              
              /* Merge into prefixTree2d and prefixTree3d */
              ret = mergeIntoGraphs(currentGraph, last_trace);
              if (ret != STAT_OK)
                 return ret;
           } /* for iter */

           gl_err = graphlib_exportGraph((char *) "origgraph.dot", GRF_DOT, prefixTree3d_);
           if (GRL_IS_FATALERROR(gl_err))
              fprintf(stderr, "Error exporting graph\n");
        }
        
        /* Continue the process */
        if (!last_trace || wasRunning) {
           ret = Resume();
           if (ret != STAT_OK)
              printMsg(ret, __FILE__, __LINE__, "Failed to resume processes\n");
        }

        if (!last_trace)
            usleep(traceFrequency * 1000);
    } /* for i */

    if (extractVariables != NULL) {
        free(extractVariables);
        extractVariables = NULL;
    }

    return STAT_OK;
}

std::string STAT_BackEnd::getFrameName(const Frame &frame, bool is_final_frame)
{
   string name;
   Address addr;
   char buf[BUFSIZE];
   char fileName[BUFSIZE];
   StatError_t statError;
   int lineNum;

   bool ret = frame.getName(name);

   /* Handle an error or empty frame name */
   if (ret == false)
      name = "[unknown]";
   else if (name == "")
      name = "[empty]";

   /* Gather PC value or line number info if requested */
   if (sampleType == STAT_FUNCTION_AND_PC)
   {
      if (is_final_frame)
         snprintf(buf, BUFSIZE, "@0x%lx", frame.getRA());
      else
         snprintf(buf, BUFSIZE, "@0x%lx", frame.getRA()-1);
      name += buf;
   }
   else if (sampleType == STAT_FUNCTION_AND_LINE)
   {
      if (is_final_frame)
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
#warning TODO: Re-enable variables
      /*if (extractVariables != NULL)
      {
         for (k = 0; k < nVariables; k++)
         {
            if (strcmp(fileName, extractVariables[k].fileName) == 0 && 
                lineNum == extractVariables[k].lineNum && 
                partial.size() - i + 1 == extractVariables[k].depth)
            {
               statError = getVariable(partial, i, extractVariables[k].variableName, buf);
               if (statError != STAT_OK)
               {
                  printMsg(statError, __FILE__, __LINE__, "Failed to get variable information for $%s in %s\n", extractVariables[k].variableName, name.c_str());
                  continue;
               }
               name += buf;
            }
         }
      }*/
   }

   return name;
}

void STAT_BackEnd::fillInName(const string &s, graphlib_nodeattr_t &nodeattr)
{
#ifdef GRL_DYNAMIC_NODE_NAME
   nodeattr.name = (char *) s.c_str();
#else
   if (strlen(s) >= GRL_MAX_NAME_LENGTH) {
      snprintf(nodeattr.name, GRL_MAX_NAME_LENGTH, "...%s",
               s.substr(trace[i].length() - GRL_MAX_NAME_LENGTH + 4, GRL_MAX_NAME_LENGTH - 2).c_str());
   }
   else {
      snprintf(nodeattr.name, GRL_MAX_NAME_LENGTH, "%s", s.c_str());
   }
#endif
}

StatError_t STAT_BackEnd::getStackTrace(graphlib_graph_p retGraph, Walker *proc, int rank, unsigned int nRetries, unsigned int retryFrequency, unsigned int withThreads)
{
    int nodeId, prevId, i, j, k, partialVal, pos;
    bool ret;
    string name, path, tester;
    vector<Frame> swalk, partial;
    vector<string> trace;
    graphlib_error_t gl_err;
    graphlib_nodeattr_t nodeattr = {1,0,20,GRC_LIGHTGREY,0,0,(char *) "",GRAPH_FONT_SIZE};
    graphlib_edgeattr_t edgeattr = {1,0,NULL,0,0,0};
    graphlib_graph_p currentGraph = NULL;
    vector<Dyninst::THR_ID> threads;
    ProcDebug *processState = NULL;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Gathering trace from task rank %d\n", rank);

    /* Set edge label */
    gl_err = graphlib_setEdgeByTask(&edgeattr.edgelist, rank);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to set edge by rank\n");
        return STAT_GRAPHLIB_ERROR;
    }

    /* If we're not attached return a trace denoting the task has exited */
    if (proc == NULL)
        threads.push_back(NULL_THR_ID);
    else
    {
        processState = dynamic_cast<ProcDebug *>(proc->getProcessState());
        if (processState == NULL)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Walker contains no Process State object\n");
            return STAT_STACKWALKER_ERROR;
        }
        else
        {
            if (processState->isTerminated() || withThreads == 0)
                threads.push_back(NULL_THR_ID);
            else
            {
                ret = proc->getAvailableThreads(threads);
                if (ret != true)
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
        currentGraph = createRootedGraph();
        if (!currentGraph) {
           return STAT_GRAPHLIB_ERROR;
        }

        path = "";
        if (proc == NULL)
        {
            /* We're not attached so return a trace denoting the task has exited */
            trace.push_back("[task_exited]");
        }
        else
        {
            /* Get the stack trace */
            partialVal = 0;
            for (i = 0; i <= nRetries; i++)
            {
                /* Try to walk the stack */
                swalk.clear();
                ret = proc->walkStack(swalk, threads[j]);
                if (ret == false && swalk.size() < 1)
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
                ret = swalk[swalk.size() - 1].getName(name);
                if (ret == false)
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
                for (i = partial.size() - 1; i >= 0; i = i - 1)
                {
                   name = getFrameName(partial[i], i == 0);
                   trace.push_back(name);
                }
            }
        } /* else proc == NULL */

        /* Get the name of the frames and add it to the graph */
        for (i = 0; i < trace.size(); i++)
        {
            /* Add \ character before '<' and '>' characters for dot format */
            tester = trace[i];
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

            /* Add the node and edge to the graph */
            path += trace[i].c_str();
            nodeId = string_hash(path.c_str());
            fillInName(trace[i], nodeattr);
            gl_err = graphlib_addNode(currentGraph, nodeId, &nodeattr);

            if (GRL_IS_FATALERROR(gl_err))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error adding node to graph\n");
                return STAT_GRAPHLIB_ERROR;
            }
            gl_err = graphlib_addDirectedEdge(currentGraph, prevId, nodeId, &edgeattr);
            if (GRL_IS_FATALERROR(gl_err))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error adding edge to graph\n");
                return STAT_GRAPHLIB_ERROR;
            }
            prevId = nodeId;
        }

        /* Merge cur graph into retGraph */
        gl_err = graphlib_mergeGraphsRanked(retGraph, currentGraph);
        if (GRL_IS_FATALERROR(gl_err))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error merging graphs\n");
            return STAT_GRAPHLIB_ERROR;
        }

        /* Free up current graph */
        gl_err = graphlib_delGraph(currentGraph);
        if (GRL_IS_FATALERROR(gl_err))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
            return STAT_GRAPHLIB_ERROR;
        }

    } /* for thread iter */

    gl_err = graphlib_delEdgeAttr(edgeattr);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to free edge attr\n");
        return STAT_GRAPHLIB_ERROR;
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Trace successfully gathered from task rank %d\n", rank);
    return STAT_OK;
}

#if defined(GROUP_OPS)

#warning TODO: Remove bitvec internal calls after merging graphlib cleanup
extern int bitvec_initialize(int longsize, int edgelabelwidth);
extern void bitvec_insert(void *vec, int val);
extern void *bitvec_allocate();

void STAT_BackEnd::AddFrameToGraph(graphlib_graph_p gl_graph, CallTree *sw_graph,
                                   graphlib_node_t gl_node, FrameNode *sw_node,
                                   string node_id_names,
                                   set<pair<Walker *, THR_ID> > *error_threads,
                                   set<int> &output_ranks)
{
   /* Add the Frame associated with FrameNode to the graphlib graph, below
      give gl_node

      Note: Lots of complexity here to deal with adding edge labels (all the std::set work).
      We could get rid of this if graphlib had graph traversal functions.
   */
   graphlib_error_t gl_err;
   frame_set_t &children = sw_node->getChildren();

   map<string, set<FrameNode*> > children_names;

   //Traversal 1: Get names for each child in the FrameNode graph.  Merge like-names
   // together into the children_names map.
   for (frame_set_t::iterator i = children.begin(); i != children.end(); i++) {
      FrameNode *child = *i;
      Frame *frame = child->getFrame();
      if (!frame) {
         //We hit a thread, that means the end of the callstack.
         // Add the associated rank to our edge label set.

         THR_ID thrd = child->getThread();
         assert(thrd != NULL_LWP);
         Walker *walker = child->getWalker();
         assert(walker);

         if (error_threads && child->hadError()) {
            //This stackwalk ended in an error.  Don't add it to the tree,
            // instead return it via error_threads.  This may lead to 
            // edges with no associated ranks, those are cleaned below.
            error_threads->insert(make_pair(walker, thrd));
            continue;
         }

         map<Walker *, int>::iterator j = procsToRanks_.find(walker);
         assert(j != procsToRanks_.end());
         int rank = j->second;
         output_ranks.insert(rank);
         continue;
      }

      string name = getFrameName(*frame, frame->isTopFrame());
      children_names[name].insert(child);
   }

   //Traversal 2: For each unique name, create a new graphlib node and add it
   for (map<string, set<FrameNode*> >::iterator i = children_names.begin(); i != children_names.end(); i++) {
      string name = i->first;
      set<FrameNode *> &kids = i->second;

      string new_node_id_names = node_id_names + name;
      int newchild = string_hash(new_node_id_names.c_str());

      if (sw_node->isHead()) {
         //Don't create a node for head, just using existing node.
         newchild = gl_node;
      }
      else {
         graphlib_nodeattr_t nodeattr = {1,0,20,GRC_LIGHTGREY,0,0, (char *) "", GRAPH_FONT_SIZE};
         fillInName(name, nodeattr);
         gl_err = graphlib_addNode(gl_graph, newchild, &nodeattr);
         if (GRL_IS_FATALERROR(gl_err)) {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add node\n");
            continue;
         }
      }
      
      //Traversal 2.1: For each new node, recursively add its children to the graph
      set<int> my_ranks;
      for (set<FrameNode *>::iterator j = kids.begin(); j != kids.end(); j++) {
         std::set<int> kids_ranks;
         FrameNode *child = *j;
         AddFrameToGraph(gl_graph, sw_graph, newchild, child, new_node_id_names, error_threads, kids_ranks);
         assert(!kids_ranks.empty());
         my_ranks.insert(kids_ranks.begin(), kids_ranks.end());
      }

      if (my_ranks.empty()) {
         //This node only had error stackwalks ending below it, thus no ranks.
         // It doesn't deserve to live.
         gl_err = graphlib_deleteConnectedNode(gl_graph, newchild);
         if (GRL_IS_FATALERROR(gl_err))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to cleanup nodes\n");
         continue;
      }
      
      //Create the edge between this new node and our parent
      graphlib_edgeattr_t edgeattr = {1,0,NULL,0,0,0};
      edgeattr.edgelist = bitvec_allocate();
      for (set<int>::iterator j = my_ranks.begin(); j != my_ranks.end(); j++) {
         bitvec_insert(edgeattr.edgelist, *j);
      }

      if (gl_node != newchild) {
         gl_err = graphlib_addDirectedEdge(gl_graph, gl_node, newchild, &edgeattr);
         if (GRL_IS_FATALERROR(gl_err)) {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add edge\n");
            continue;
         }
      }

      output_ranks.insert(my_ranks.begin(), my_ranks.end());
   }
}

StatError_t STAT_BackEnd::getStackTraceFromAll(graphlib_graph_p retGraph,
                                               unsigned int /*nRetries*/, unsigned int /*retryFrequency*/,
                                               unsigned int /*withThreads*/)
{
   CallTree tree(frame_lib_offset_cmp);

   bool result = walkerset->walkStacks(tree);
   if (!result) {
#warning TODO: Start handling partial call stacks in group walks
   }

   size_t setsize = procset->size();
   size_t proc_bitfield_size = setsize;
   proc_bitfield_size = proc_bitfield_size / sizeof(char);
   if (setsize % sizeof(char)) proc_bitfield_size++;
   bitvec_initialize(0, proc_bitfield_size);
   
   string empty;
   set<int> ranks;
   AddFrameToGraph(retGraph, &tree,
                   0, tree.getHead(), empty, NULL, ranks);

   return STAT_OK;
}
#endif

StatError_t STAT_BackEnd::Detach(unsigned int *stopArray, int stopArrayLen)
{
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Detaching from application processes\n");
    int i;
    bool leaveStopped;
    map<int, Walker *>::iterator iter;
#if defined(GROUP_OPS)
    if (doGroupOps_) {
       procset->detach();
    }
    else
#endif
    {
       for (iter = processMap_.begin(); iter != processMap_.end(); iter++)
       {
          leaveStopped = false;
          if (iter->second != NULL)
          {
             ProcDebug *pdebug = dynamic_cast<ProcDebug *>(iter->second->getProcessState());
             if (pdebug == NULL)
             {
                printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to dynamic_cast ProcDebug pointer\n");
                return STAT_STACKWALKER_ERROR;
             }
#ifndef BGL            
             for (i = 0; i < stopArrayLen; i++)
             {
                if (stopArray[i] == iter->first)
                {
                   leaveStopped = true;
                   break;
                }
             }            
#endif            
             pdebug->detach(leaveStopped);
             delete iter->second;
          }
          processMap_.erase(iter);
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

int Unpack_STAT_BE_info(void *buf, int bufLen, void *data)
{
    statLeafInfoArray_t *leafInfoArray = (statLeafInfoArray_t *)data;
    int i, j, nChildren, nParents, k, currentParentPort, currentParentRank;
    char *ptr = (char *)buf, currentParent[STAT_MAX_BUF_LEN];

    /* Get the number of daemons and set up the leaf info array */
    memcpy((void *)&(leafInfoArray->size), ptr, sizeof(int));
    ptr += sizeof(int);
    leafInfoArray->leaves = (statLeafInfo_t *)malloc(leafInfoArray->size * sizeof(statLeafInfo_t));
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

unsigned int string_hash(const char *str)
{
    unsigned int hash = 0;
    int c;

    while (c = *(str++))
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
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
//    {
//        MRN_DEBUG_LOG_DIRECTORY = NULL;
//        stderr = statOutFp;
//    }
    return STAT_OK;
}

void STAT_BackEnd::printMsg(StatError_t statError, const char *sourceFile, int sourceLine, const char *fmt, ...)
{
    va_list args;
    char timeString[BUFSIZE];
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
            char msg[BUFSIZE];
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


/******************
 * STATBench Code *
 ******************/

StatError_t STAT_BackEnd::statBenchConnectInfoDump()
{
    int i, count, ret, fd;
    unsigned int bytesWritten = 0;
    char fileName[BUFSIZE], data[BUFSIZE], *ptr;
    struct stat buf;
    lmon_rc_e rc;
    statLeafInfoArray_t leafInfoArray;

    /* Master daemon receive MRNet connection information */
    rc = LMON_be_recvUsrData((void *)&leafInfoArray);
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to receive data from FE\n");
        return STAT_LMON_ERROR;
    }

    /* Master broadcast number of daemons */
    rc = LMON_be_broadcast((void *)&(leafInfoArray.size), sizeof(int));
    if (rc != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to broadcast num_leaves\n");
        return STAT_LMON_ERROR;
    }

    /* Non-masters allocate space for the MRNet connection info */
    if (LMON_be_amIMaster() == LMON_NO)
    {
        leafInfoArray.leaves = (statLeafInfo_t *)malloc(leafInfoArray.size * sizeof(statLeafInfo_t));
        if (leafInfoArray.leaves == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to allocate memory for leaf info\n");
            return STAT_ALLOCATE_ERROR;
        }
    }

    /* Master broadcast MRNet connection information to all daemons */
    rc = LMON_be_broadcast((void *)(leafInfoArray.leaves), leafInfoArray.size * sizeof(statLeafInfo_t));
    if (rc != LMON_OK)
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
        string prettyHost, leafPrettyHost;
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
                ret = stat(fileName, &buf);
                if (ret == 0)
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
                ret = write(fd, ptr, strlen(ptr));
                if (ret == -1)
                {
                    printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: write() to fifo %s failed\n", strerror(errno), fileName);
                    remove(fileName);
                    return STAT_FILE_ERROR;
                }
                bytesWritten += ret;
            }

            /* Close the file descriptor */
            ret = close(fd);
            if (ret != 0)
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
        ret = remove(fileName);
        if (ret != 0)// && errno != ENOENT)
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
    int i, ret, fd, bytesRead = 0, inPort, inParentRank, inRank, mpiRank;
    char fileName[BUFSIZE], inHostName[BUFSIZE], data[BUFSIZE], *ptr;

    for (i = 0; i < 8192; i++)
    {
        /* Set up my file name */
        snprintf(fileName, BUFSIZE, "/tmp/%s.%d.statbench.txt", localHostName_, i);
    
        /* Make the fifo if the helper hasn't already done so */
        ret = mkfifo(fileName, S_IRUSR | S_IWUSR);
        if (ret != 0)
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
        ret = read(fd, ptr, BUFSIZE - bytesRead);
        if (ret == -1)
        {
            printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: read() from fifo %s fd %d failed\n", strerror(errno), fileName, fd);
            remove(fileName);
            return STAT_FILE_ERROR;
        }
        bytesRead += ret;
    } while (ret > 0);

    /* Extract the information from the received string */
    if (sscanf(data, "%s %d %d %d %d", inHostName, &inPort, &inParentRank, &inRank, &mpiRank) == -1)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: sscanf() failed to find MRNet connection info\n", strerror(errno));
        remove(fileName);
        return STAT_MRNET_ERROR;
    }
    parentHostName_ = strdup(inHostName);
    parentPort_ = inPort;
    parentRank_ = inParentRank;
    myRank_ = inRank;

    /* Close the fifo fd */
    ret = close(fd);
    if (ret == -1)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: close() failed for fifo %s, fd %d\n", strerror(errno), fileName, fd);
        remove(fileName);
        return STAT_FILE_ERROR;
    }

    /* Connect to the MRNet Network */
#ifdef MRNET22
    char *param[6], parentPort[BUFSIZE], parentRank[BUFSIZE], myRank[BUFSIZE];
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
#else
    network_ = new Network(parentHostName_, parentPort_, parentRank_, localHostName_, myRank_);
#endif
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
    graphlib_graph_p currentGraph;
    graphlib_error_t gl_err;
    unsigned int i, j;
    static int init = 0;

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
        }
        init++;
        gl_err = graphlib_InitVarEdgeLabels(nTasks);
        if (GRL_IS_FATALERROR(gl_err))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to initialize graphlib\n");
            return STAT_GRAPHLIB_ERROR;
        }
    }

    /* Delete previously merged graphs */
    if (prefixTree3d_ != NULL)
    {
        gl_err = graphlib_delGraph(prefixTree3d_);
        if (GRL_IS_FATALERROR(gl_err))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
    }
    if (prefixTree2d_ != NULL)
    {
        gl_err = graphlib_delGraph(prefixTree2d_);
        if (GRL_IS_FATALERROR(gl_err))
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
    }

    /* Create graphs */
    gl_err = graphlib_newGraph(&prefixTree3d_);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error initializing 3D graph\n");
        return STAT_GRAPHLIB_ERROR;
    }
    gl_err = graphlib_newGraph(&prefixTree2d_);
    if (GRL_IS_FATALERROR(gl_err))
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
            /* Create the trace for this task */
            currentGraph = statBenchCreateTrace(maxDepth, j, functionFanout, nEqClasses, i);
            if (currentGraph == NULL)
            {
                printMsg(STAT_SAMPLE_ERROR, __FILE__, __LINE__, "Error creating trace\n");
                return STAT_SAMPLE_ERROR;
            }

            /* Merge cur graph into retGraph */
            gl_err = graphlib_mergeGraphsRanked(prefixTree3d_, currentGraph);
            if (GRL_IS_FATALERROR(gl_err))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error merging graphs\n");
                return STAT_GRAPHLIB_ERROR;
            }

            /* Merge the current graph for 2D analysis */
            if (i == 0)
            {
                gl_err = graphlib_mergeGraphsRanked(prefixTree2d_, currentGraph);
                if (GRL_IS_FATALERROR(gl_err))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error merging graphs\n");
                    return STAT_GRAPHLIB_ERROR;
                }
            }

            /* Free up the temporary graph if it's not being stored as an eq class */
            if (nEqClasses == -1)
            {
                gl_err = graphlib_delGraph(currentGraph);
                if (GRL_IS_FATALERROR(gl_err))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
                    return STAT_GRAPHLIB_ERROR;
                }
            }
        }
    }

    return STAT_OK;
}

graphlib_graph_p STAT_BackEnd::statBenchCreateTrace(unsigned int maxDepth, unsigned int task, unsigned int functionFanout, int nEqClasses, unsigned int iter)
{
    int depth, i, nodeId, prevId;
    char temp[8192];
    static vector<graphlib_graph_p> generated_graphs;
    string path;
    graphlib_graph_p retGraph;
    graphlib_error_t gl_err;
    graphlib_nodeattr_t nodeattr = {1,0,20,GRC_LIGHTGREY,0,0,NULL,GRAPH_FONT_SIZE};
    graphlib_edgeattr_t edgeattr = {1,0,NULL,0,0,0};

    /* set the edge label */
    gl_err = graphlib_setEdgeByTask(&edgeattr.edgelist, task);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "o set edge by task\n");
        return NULL;
    }

    /* See if we've already generated a graph for this equivalence class and iteration */
    if (task == 0 && nEqClasses != -1)
    {
        for (i = 0; i < generated_graphs.size(); i++)
        {
            /* Free up the graph previously cached graphs */
            gl_err = graphlib_delGraph(generated_graphs[i]);
            if (GRL_IS_FATALERROR(gl_err))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
                return NULL;
            }
        }
        generated_graphs.clear();
    }
    if (nEqClasses != -1 && task >= nEqClasses)
    {
        if (generated_graphs.size() > task % nEqClasses)
        {
            /* modify the edges with the proper rank */
            graphlib_modifyEdgeAttr(generated_graphs[task % nEqClasses], &edgeattr);
            gl_err = graphlib_delEdgeAttr(edgeattr);
            if (GRL_IS_FATALERROR(gl_err))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to free edge attr\n");
                return NULL;
            }
            return generated_graphs[task % nEqClasses];
        }
    }

    /* create return graph */
    /* Create graph root */
    retGraph = createRootedGraph();
    if (!retGraph)
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to create new graph\n");
        return NULL;
    }

    path = "/";
    prevId = 0;

    /* Create artificial libc_start_main */
    fillInName("__libc_start_main", nodeattr);
    path += nodeattr.name;
    nodeId = string_hash(path.c_str());
    gl_err = graphlib_addNode(retGraph, nodeId, &nodeattr);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add node\n");
        return NULL;
    }
    gl_err = graphlib_addDirectedEdge(retGraph, prevId, nodeId, &edgeattr);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add edge\n");
        return NULL;
    }
    prevId = nodeId;

    /* Create artificial main */
    fillInName("main", nodeattr);
    path += nodeattr.name;
    nodeId = string_hash(path.c_str());
    gl_err = graphlib_addNode(retGraph, nodeId, &nodeattr);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add node\n");
        return NULL;
    }
    gl_err = graphlib_addDirectedEdge(retGraph, prevId, nodeId, &edgeattr);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add edge\n");
        return NULL;
    }
    prevId = nodeId;

    /* Seed the random number generator based on my equivalence class */
    if (nEqClasses == -1) /* no limit */
        srand(task + 999999 * iter);
    else
        srand(task % nEqClasses +  999999 * iter);

    /* Generate a atack trace and add it to the graph */
    depth = rand() % maxDepth;
    for (i = 0; i < depth; i++)
    {
        snprintf(temp, 8192, "depth%dfun%d", i, rand() % functionFanout);
        fillInName(temp, nodeattr);

        path += nodeattr.name;
        nodeId = string_hash(path.c_str());
        gl_err = graphlib_addNode(retGraph, nodeId, &nodeattr);

        if (GRL_IS_FATALERROR(gl_err))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add node\n");
            return NULL;
        }
        gl_err = graphlib_addDirectedEdge(retGraph, prevId, nodeId, &edgeattr);
        if (GRL_IS_FATALERROR(gl_err))
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to add edge\n");
            return NULL;
        }
        prevId = nodeId;
    }
    gl_err = graphlib_delEdgeAttr(edgeattr);
    if (GRL_IS_FATALERROR(gl_err))
    {
        printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to free edge attr\n");
        return NULL;
    }

    if (nEqClasses != -1)
        generated_graphs.push_back(retGraph);
    return retGraph;
}

