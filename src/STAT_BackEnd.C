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

FILE *gStatOutFp = NULL;

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

    processMapNonNull_ = 0;
    gStatOutFp = NULL;
    initialized_ = false;
    connected_ = false;
    isRunning_ = false;
    network_ = NULL;
    proctab_ = NULL;
    proctabSize_ = 0;
    parentHostName_ = NULL;
    broadcastStream_ = NULL;
    sampleType_ = 0;
#ifdef STAT_FGFS
    fgfsCommFabric_ = NULL;
#endif
}

STAT_BackEnd::~STAT_BackEnd()
{
    int i;
    graphlib_error_t graphlibError;
    map<int, StatBitVectorEdge_t *>::iterator nodeInEdgeMapIter;

    /* delete stored nodes and edges */
    clear2dNodesAndEdges();
    clear3dNodesAndEdges();

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

void STAT_BackEnd::clear2dNodesAndEdges()
{
    map<int, pair<int, StatBitVectorEdge_t *> >::iterator edgesIter;
    nodes2d_.clear();
    for (edgesIter = edges2d_.begin(); edgesIter != edges2d_.end(); edgesIter++)
        if (edgesIter->second.second != NULL)
            statFreeEdge(edgesIter->second.second);
    edges2d_.clear();
}

void STAT_BackEnd::clear3dNodesAndEdges()
{
    map<int, pair<int, StatBitVectorEdge_t *> >::iterator edgesIter;
    nodes3d_.clear();
    for (edgesIter = edges3d_.begin(); edgesIter != edges3d_.end(); edgesIter++)
        if (edgesIter->second.second != NULL)
            statFreeEdge(edgesIter->second.second);
    edges3d_.clear();
}


StatError_t STAT_BackEnd::update3dNodesAndEdges()
{
    map<int, string>::iterator nodesIter;
    map<int, pair<int, StatBitVectorEdge_t *> >::iterator edgesIter;
    StatBitVectorEdge_t *edge;

    for (nodesIter = nodes2d_.begin(); nodesIter != nodes2d_.end(); nodesIter++)
    {
        nodes3d_[nodesIter->first] = nodesIter->second;
    }

    for (edgesIter = edges2d_.begin(); edgesIter != edges2d_.end(); edgesIter++)
    {
        if (edges3d_.find(edgesIter->first) == edges3d_.end())
        {
            edge = initializeBitVectorEdge(proctabSize_);
            if (edge == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to initialize edge\n");
                return STAT_ALLOCATE_ERROR;
            }
            edges3d_[edgesIter->first] = make_pair(edgesIter->second.first, edge);
        }
        statMergeEdge(edges3d_[edgesIter->first].second, edgesIter->second.second);
    }

    return STAT_OK;
}


StatError_t STAT_BackEnd::update2dEdge(int src, int dst, StatBitVectorEdge_t *edge)
{
    StatBitVectorEdge_t *newEdge;

    if (edges2d_.find(dst) == edges2d_.end())
    {
        newEdge = initializeBitVectorEdge(proctabSize_);
        if (newEdge == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to initialize newEdge\n");
            return STAT_ALLOCATE_ERROR;
        }
        edges2d_[dst] = make_pair(src, newEdge);
    }
    statMergeEdge(edges2d_[dst].second, edge);

    return STAT_OK;
}

StatError_t STAT_BackEnd::generateGraphs(graphlib_graph_p *prefixTree2d, graphlib_graph_p *prefixTree3d)
{
    int i;
    StatCountRepEdge_t *countRepEdge;
    graphlib_graph_p *currentGraph;
    graphlib_error_t graphlibError;
    graphlib_nodeattr_t nodeAttr = {1,0,20,GRC_LIGHTGREY,0,0,(char *)"",-1};
    graphlib_edgeattr_t edgeAttr = {1,0,NULL,0,0,0};
    map<int, string> *nodes;
    map<int, pair<int, StatBitVectorEdge_t *> > *edges;
    map<int, string>::iterator nodesIter;
    map<int, pair<int, StatBitVectorEdge_t *> >::iterator edgesIter;

    for (i = 0; i < 2; i++)
    {
        if (i == 0)
        {
            currentGraph = prefixTree2d;
            nodes = &nodes2d_;
            edges = &edges2d_;
        }
        else
        {
            currentGraph = prefixTree3d;
            nodes = &nodes3d_;
            edges = &edges3d_;
        }

        /* Delete previously merged graphs */
        if (*currentGraph != NULL)
        {
            graphlibError = graphlib_delGraph(*currentGraph);
            if (GRL_IS_FATALERROR(graphlibError))
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error deleting graph\n");
        }

        /* Create graphs */
        *currentGraph = createRootedGraph(sampleType_);
        if (*currentGraph == NULL)
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error initializing graph\n");
            return STAT_GRAPHLIB_ERROR;
        }

        for (nodesIter = (*nodes).begin(); nodesIter != (*nodes).end(); nodesIter++)
        {
            nodeAttr.label = (void *)nodesIter->second.c_str();
            graphlibError = graphlib_addNode(*currentGraph, nodesIter->first, &nodeAttr);
            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error adding node to graph\n");
                return STAT_GRAPHLIB_ERROR;
            }
        }
        for (edgesIter = (*edges).begin(); edgesIter != (*edges).end(); edgesIter++)
        {
            if (sampleType_ & STAT_SAMPLE_COUNT_REP)
            {
                countRepEdge = getBitVectorCountRep(edgesIter->second.second);
                if (countRepEdge == NULL)
                {
                    printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to translate bit vector into count + representative\n");
                    return STAT_ALLOCATE_ERROR;
                }
                edgeAttr.label = (void *)countRepEdge;
            }
            else
                edgeAttr.label = (void *)edgesIter->second.second;
            graphlibError = graphlib_addDirectedEdge(*currentGraph, edgesIter->second.first, edgesIter->first, &edgeAttr);
            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error adding edge to graph\n");
                return STAT_GRAPHLIB_ERROR;
            }
            if (sampleType_ & STAT_SAMPLE_COUNT_REP)
            {
                graphlibError = graphlib_delEdgeAttr(edgeAttr, statFreeCountRepEdge);
                if (GRL_IS_FATALERROR(graphlibError))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to free edge attr\n");
                    return STAT_GRAPHLIB_ERROR;
                }
            }
        }
    }

    return STAT_OK;
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
    } /* if (argc == 0 || argv == NULL) */
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
    } /* else (argc == 0 || argv == NULL) */

    connected_ = true;

    return STAT_OK;
}

StatError_t STAT_BackEnd::mainLoop()
{
    int tag = 0, intRet, nEqClasses, swNotificationFd, mrnNotificationFd, maxFd, nodeId, bitVectorLength, stopArrayLen, major, minor, revision;
    unsigned int nTraces, traceFrequency, nTasks, functionFanout, maxDepth, nRetries, retryFrequency, *stopArray = NULL, previousSampleType = 0;
    uint64_t obyteArrayLen;
    char *obyteArray = NULL, *variableSpecification = NULL;
    bool boolRet, shouldBlock;
    Stream *stream = NULL;
    fd_set readFds, writeFds, exceptFds;
    PacketPtr packet;
    StatError_t statError;
    graphlib_error_t graphlibError;
    graphlib_graph_p prefixTree2d = NULL, prefixTree3d = NULL;
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
                if (packet->unpack("%ud %ud %ud %ud %ud %s", &nTraces, &traceFrequency, &nRetries, &retryFrequency, &sampleType_, &variableSpecification) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_SAMPLE_TRACES) failed\n");
                    if (sendAck(stream, PROT_SAMPLE_TRACES_RESP, intRet) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_SAMPLE_TRACES_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }
                    return STAT_MRNET_ERROR;
                }
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to sample %d traces each %d us with %d retries every %d us with variables %s\n", nTraces, traceFrequency, nRetries, retryFrequency, variableSpecification);

                /* When switching between count + rep and full list, we need to clear previous samples */
                if ((previousSampleType ^ sampleType_) & STAT_SAMPLE_COUNT_REP)
                    sampleType_ |= STAT_SAMPLE_CLEAR_ON_SAMPLE;
                previousSampleType = sampleType_;

                /* Gather stack traces */
                statError = sampleStackTraces(nTraces, traceFrequency, nRetries, retryFrequency, variableSpecification);
                if (statError == STAT_OK)
                    intRet = 0;

                statError = generateGraphs(&prefixTree2d, &prefixTree3d);
                if (statError != STAT_OK)
                    intRet = 1;

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
                graphlibError = graphlib_serializeBasicGraph(prefixTree2d, &obyteArray, &obyteArrayLen);
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
                graphlibError = graphlib_serializeBasicGraph(prefixTree3d, &obyteArray, &obyteArrayLen);
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
                if (edges2d_.find(nodeId) != edges2d_.end())
                    edge = edges2d_[nodeId].second;
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
                if (packet->unpack("%ud %ud %ud %ud %d %ud", &maxDepth, &nTasks, &nTraces, &functionFanout, &nEqClasses, &sampleType_) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_STATBENCH_CREATE_TRACES) failed\n");
                    if (sendAck(stream, PROT_STATBENCH_CREATE_TRACES_RESP, intRet) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_STATBENCH_CREATE_TRACES_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }
                    return STAT_MRNET_ERROR;
                }
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to create traces with max depth = %d, num tasks = %d, num traces = %d, function fanout = %d, equivalence classes = %d, sample type = %u\n", maxDepth, nTasks, nTraces, functionFanout, nEqClasses, sampleType_);

                /* Create stack traces */
                statError = statBenchCreateTraces(maxDepth, nTasks, nTraces, functionFanout, nEqClasses);
                if (statError == STAT_OK)
                    intRet = 0;

                statError = generateGraphs(&prefixTree2d, &prefixTree3d);
                if (statError != STAT_OK)
                    intRet = 1;

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
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Failed to initialize AsyncswDebugFile_Status\n");
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
#endif /*FGFS */

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

#if defined(GROUP_OPS)
        if (doGroupOps_)
        {
            Process::ptr pcProc = aInfo[i].proc;
            proc = pcProc ? Walker::newWalker(pcProc) : NULL;
            if (proc != NULL)
            {
                pcProc->setData(proc); /* Up ptr for mapping Process::ptr -> Walker */
                walkerSet_->insert(proc);
            }
        }
        else
#endif
        {
            proc = Walker::newWalker(proctab_[i].pd.pid, proctab_[i].pd.executable_name);
        }

        if (proc == NULL)
        {
            printMsg(STAT_WARNING, __FILE__, __LINE__, "StackWalker Attach to task rank %d, pid %d failed with message '%s'\n", proctab_[i].mpirank, proctab_[i].pd.pid, Dyninst::Stackwalker::getLastErrorMsg());
            swDebugBufferToFile();
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
#if defined(PROTOTYPE_TO) || defined(PROTOTYPE_PY)
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


StatError_t STAT_BackEnd::sampleStackTraces(unsigned int nTraces, unsigned int traceFrequency, unsigned int nRetries, unsigned int retryFrequency, char *variableSpecification)
{
    int j;
    unsigned int i;
    bool wasRunning = isRunning_, isLastTrace;
    graphlib_error_t graphlibError;
    StatError_t statError;
    map<int, Walker *>::iterator processMapIter;
    map<int, StatBitVectorEdge_t *>::iterator nodeInEdgeMapIter;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Preparing to sample stack traces\n");

    if (sampleType_ & STAT_SAMPLE_CLEAR_ON_SAMPLE)
        clear3dNodesAndEdges();

    /* Parse the variable specification */
    if (strcmp(variableSpecification, "NULL") != 0)
    {
        statError = parseVariableSpecification(variableSpecification);
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Failed to parse variable specification\n");
            return statError;
        }
    }

    /* Gather stack traces and merge */
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Gathering and merging %d traces from each task\n", nTraces);
    for (i = 0; i < nTraces; i++)
    {
        clear2dNodesAndEdges();
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
            getStackTraceFromAll(nRetries, retryFrequency);
        else
#endif
        {
            for (processMapIter = processMap_.begin(), j = 0; processMapIter != processMap_.end(); processMapIter++, j++)
            {
                statError = getStackTrace(processMapIter->second, j, nRetries, retryFrequency);
                if (statError != STAT_OK)
                {
                    printMsg(statError, __FILE__, __LINE__, "Error getting graph %d of %d\n", i + 1, nTraces);
                    return statError;
                }
            }
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

        statError = update3dNodesAndEdges();
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Error updating 3d nodes and edges for trace %d of %d\n", i + 1, nTraces);
            return statError;
        }
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
    if (sampleType_ & STAT_SAMPLE_PYTHON && name.find("PyEval_EvalFrameEx") != string::npos)
    {
        statError = getPythonFrameInfo(frame.getWalker(), frame, &pyFun, &pySource, &pyLineNo);
        if (statError == STAT_OK && pyFun != NULL && pySource != NULL)
        {
            if (sampleType_ & STAT_SAMPLE_LINE)
            {
                snprintf(buf, BUFSIZE, "%s@%s:%d", pyFun, pySource, pyLineNo);
                name = buf;
            }
            else
                name = pyFun;
            free(pyFun);
            free(pySource);
            isPyTrace_ = true;
            return name;
        }
        else if (statError != STAT_OK)
            printMsg(statError, __FILE__, __LINE__, "Error in getPythonFrameInfo for frame %s, pyFun = %s, pySource = %s\n", name.c_str(), pyFun, pySource);
    }

    /* Gather PC value or line number info if requested */
    if (sampleType_ & STAT_SAMPLE_PC)
    {
        if (frame.isTopFrame() == true)
            snprintf(buf, BUFSIZE, "@0x%lx", frame.getRA());
        else
            snprintf(buf, BUFSIZE, "@0x%lx", frame.getRA() - 1);
        name += buf;
    }
    else if (sampleType_ & STAT_SAMPLE_LINE)
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
                if (strcmp(fileName, extractVariables_[i].fileName) == 0 &&
                    lineNum == extractVariables_[i].lineNum &&
                    depth == extractVariables_[i].depth)
                {
                    statError = getVariable(frame, extractVariables_[i].variableName, buf, BUFSIZE);
                    value = *((int *)buf);
                    snprintf(buf, BUFSIZE, "$%s=%d", extractVariables_[i].variableName, value);
                    if (statError != STAT_OK)
                    {
                       printMsg(statError, __FILE__, __LINE__, "Failed to get variable information for $%s in %s\n", extractVariables_[i].variableName, name.c_str());
                       continue;
                    }
                    name += buf;
                }
            }
        }
    }

    return name;
}


StatError_t STAT_BackEnd::getStackTrace(Walker *proc, int rank, unsigned int nRetries, unsigned int retryFrequency)
{
    int nodeId, prevId, i, j, partialVal, pos, pyLineNo;
    bool boolRet, isFirstPythonFrame;
    char *pyFun = NULL, *pySource = NULL, outLine[BUFSIZE];
    string name, path;
    vector<Frame> currentStackWalk, bestStackWalk;
    vector<string> trace, outTrace;
    graphlib_error_t graphlibError;
    StatBitVectorEdge_t *edge;
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
            if (pDebug->isTerminated() || !(sampleType_ & STAT_SAMPLE_THREADS))
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
                currentStackWalk.clear();
                boolRet = proc->walkStack(currentStackWalk, threads[j]);
                if (boolRet == false && currentStackWalk.size() < 1)
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
                boolRet = currentStackWalk[currentStackWalk.size() - 1].getName(name);
                if (boolRet == false)
                {
                    if (partialVal < 1)
                    {
                        /* This is the best trace so far */
                        partialVal = 1;
                        bestStackWalk = currentStackWalk;
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
                        bestStackWalk = currentStackWalk;
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
                bestStackWalk = currentStackWalk;
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
                isPyTrace_ = false;
                for (i = bestStackWalk.size() - 1; i >= 0; i = i - 1)
                {
                    name = getFrameName(bestStackWalk[i], bestStackWalk.size() - i + 1);

                    /* Add the node and edge to the graph */
                    if (sampleType_ & STAT_SAMPLE_PYTHON)
                    {
                        if (isPyTrace_ == true && isFirstPythonFrame == true)
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
                        if (isPyTrace_ == true && (name.find("call_function") == string::npos && name.find("fast_function") == string::npos && name.find("PyEval") == string::npos))
                            trace.push_back(name);
                        else if (isPyTrace_ == false)
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
            outTrace.push_back(trace[i]);
        }

        for (i = 0; i < outTrace.size(); i++)
        {
            path += outTrace[i].c_str();
            nodeId = statStringHash(path.c_str());
            nodes2d_[nodeId] = outTrace[i];

            update2dEdge(prevId, nodeId, edge);
            prevId = nodeId;
        }
        outTrace.clear();

        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Trace from thread %d of %d = %s\n", j, threads.size(), path.c_str());

    } /* for j thread */

    statFreeEdge(edge);

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Trace successfully gathered from task rank %d\n", rank);
    return STAT_OK;
}

#if defined(GROUP_OPS)
StatError_t STAT_BackEnd::addFrameToGraph(CallTree *stackwalkerGraph, graphlib_node_t graphlibNode, FrameNode *stackwalkerNode, string nodeIdNames, set<pair<Walker *, THR_ID> > *errorThreads, set<int> &outputRanks, bool &abort, int branches)
{
    int rank, newChildId, maxAncestorBranches;
    bool myAbort = false, allMyChildrenAbort = true;
    set<int> myRanks, kidsRanks;
    set<int>::iterator myRanksIter;
    string name, newNodeIdNames;
    frame_set_t &children = stackwalkerNode->getChildren();
    frame_set_t::iterator childrenIter;
    Frame *frame;
    FrameNode *child;
    set<FrameNode *> kids;
    set<FrameNode *>::iterator kidsIter;
    map<string, set<FrameNode*> > childrenNames;
    map<string, set<FrameNode*> >::iterator childrenNamesIter;
    THR_ID threadId;
    Walker *walker;
    map<Walker *, int>::iterator procsToRanksIter;
    graphlib_error_t graphlibError;
    StatBitVectorEdge_t *edge;
    StatError_t statError;

    /* Add the Frame associated with stackwalkerNode to the graphlibGraph, below the given graphlibNode */
    /* Note: Lots of complexity here to deal with adding edge labels (all the std::set work). We could get rid of this if graphlib had graph traversal functions. */

    /* Traversal 1: Get names for each child in the FrameNode graph.  Merge like-names together into the childrenNames map. */
    maxAncestorBranches = branches;
    if (children.size() > maxAncestorBranches)
        maxAncestorBranches = children.size();
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
            allMyChildrenAbort = false;
            continue;
        }
        name = getFrameName(*frame);
        childrenNames[name].insert(child);
    }

    /* Traversal 2: For each unique name, create a new graphlib node and add it */
    for (childrenNamesIter = childrenNames.begin(); childrenNamesIter != childrenNames.end(); childrenNamesIter++)
    {
        myAbort = false;
        name = childrenNamesIter->first;
        kids = childrenNamesIter->second;

        if (isPyTrace_ == true && name.find("<module>") != string::npos)
        {
            /* Delete Python interpreter frames */
            if (branches == 1)
                clear2dNodesAndEdges(); /* There is only a single path from __start to here, so we can delete the whole graph */
            else
            {
                /* There are other paths, so just delete the parent node */
                newChildId = 0;
                if (nodes2d_.find(graphlibNode) != nodes2d_.end())
                    nodes2d_.erase(graphlibNode);
            }
            myAbort = true;
            nodeIdNames = "";
            graphlibNode = 0;
        }

        if (isPyTrace_ == true && (name.find("call_function") != string::npos || name.find("fast_function") != string::npos) || name.find("PyCFunction_Call") != string::npos)
        {
            /* We're in a python interpreter frame, don't add this node and use the previous parent node for the next edge */
            newNodeIdNames = nodeIdNames;
            newChildId = graphlibNode;
        }
        else
        {
            newNodeIdNames = nodeIdNames + name;
            newChildId = statStringHash(newNodeIdNames.c_str());
            nodes2d_[newChildId] = name;
        }

        /* Traversal 2.1: For each new node, recursively add its children to the graph */
        myRanks.clear();
        for (kidsIter = kids.begin(); kidsIter != kids.end(); kidsIter++)
        {
            kidsRanks.clear();
            child = *kidsIter;
            statError = addFrameToGraph(stackwalkerGraph, newChildId, child, newNodeIdNames, errorThreads, kidsRanks, abort, maxAncestorBranches);
            if (statError != STAT_OK)
            {
                printMsg(statError, __FILE__, __LINE__, "Error adding frame to graph\n");
                return statError;
            }
            if (abort == false)
                allMyChildrenAbort = false;
            myRanks.insert(kidsRanks.begin(), kidsRanks.end());
        }

        if (myRanks.empty())
        {
            if (nodes2d_.find(newChildId) != nodes2d_.end())
                nodes2d_.erase(newChildId);
            continue;
        }

        /* Create the edge between this new node and our parent */
        if (graphlibNode != newChildId && abort == false)
        {
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
            update2dEdge(graphlibNode, newChildId, edge);
            statFreeEdge(edge);
        } /* if (graphlibNode != newChildId) */

        if (abort == false)
            outputRanks.insert(myRanks.begin(), myRanks.end());
    } /* for (childrenNamesIter = childrenNames.begin()...) */

    abort = false;
    if (myAbort == true || allMyChildrenAbort == true)
        abort = true; /* this is what I return */
    if (allMyChildrenAbort == true && graphlibNode != 0)
    {
        if (nodes2d_.find(graphlibNode) != nodes2d_.end())
            nodes2d_.erase(graphlibNode);
    }

    return STAT_OK;
}

STAT_BackEnd *bePtr;
bool statFrameCmp(const Frame &frame1, const Frame &frame2)
{
    int depth = -1;
    string name1, name2;

#ifdef PROTOTYPE_TO
    /* TODO: compute the depth of each frame for variable extraction */
#endif
    /* TODO: cache values to avoid recomputing since this is performed many times... not quite so simple when we need to gather variables, though */
    name1 = bePtr->getFrameName(frame1, depth);
    name2 = bePtr->getFrameName(frame2, depth);

    if (name1.compare(name2) < 0)
        return true;
    return false;
}

StatError_t STAT_BackEnd::getStackTraceFromAll(unsigned int nRetries, unsigned int retryFrequency)
{
    int dummyBranches = 0;
    unsigned int i;
    bool boolRet, dummyAbort = false;
    string dummyString;
    set<int> ranks;
    StatError_t statError;

    if (procSet_->getLWPTracking() && sampleType_ & STAT_SAMPLE_THREADS)
        procSet_->getLWPTracking()->refreshLWPs();

    bePtr = this;

#if defined(PROTOTYPE_TO) || defined(PROTOTYPE_PY)
    CallTree tree(statFrameCmp);
#else
    CallTree tree(frame_lib_offset_cmp);
#endif
    boolRet = walkerSet_->walkStacks(tree, !(sampleType_ & STAT_SAMPLE_THREADS));
    if (boolRet == false)
    {
#warning TODO: Start handling partial call stacks in group walks
    }

    isPyTrace_ = false;
    statError = addFrameToGraph(&tree, 0, tree.getHead(), dummyString, NULL, ranks, dummyAbort, dummyBranches);
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

StatError_t STAT_BackEnd::startLog(unsigned int logType, char *logOutDir, int mrnetOutputLevel)
{
    char fileName[BUFSIZE];
    
    logType_ = logType;
    snprintf(logOutDir_, BUFSIZE, "%s", logOutDir);

    if (logType_ & STAT_LOG_BE || logType_ & STAT_LOG_SW)
    {
        snprintf(fileName, BUFSIZE, "%s/%s.STATD.log", logOutDir, localHostName_);
        gStatOutFp = fopen(fileName, "w");
        if (gStatOutFp == NULL)
        {
            printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: fopen failed for %s\n", strerror(errno), fileName);
            return STAT_FILE_ERROR;
        }
        if (logType_ & STAT_LOG_MRN)
            mrn_printf_init(gStatOutFp);
        set_OutputLevel(mrnetOutputLevel);
    }

    if (logType_ & STAT_LOG_SWERR || logType_ & STAT_LOG_SW)
    {
        if (logType_ & STAT_LOG_SW)
            swDebugFile_ = gStatOutFp;
        else
        {
            swDebugString_ = (char *)malloc(STAT_SW_DEBUG_BUFFER_LENGTH);
            if (swDebugString_ == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: malloc returned NULL for swDebugString_\n", strerror(errno));
                return STAT_ALLOCATE_ERROR;
            }
            swDebugFile_ = fmemopen(swDebugString_, STAT_SW_DEBUG_BUFFER_LENGTH - 1, "w");
            if (swDebugFile_ == NULL)
            {
                printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "%s: fmemopen failed\n", strerror(errno));
                return STAT_STACKWALKER_ERROR;
            }
        }
        Dyninst::Stackwalker::setDebug(true);
        Dyninst::Stackwalker::setDebugChannel(swDebugFile_);
#ifdef SW_VERSION_8_0_0
        Dyninst::ProcControlAPI::setDebug(true);
        Dyninst::ProcControlAPI::setDebugChannel(swDebugFile_);
#endif
    }

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
    if (statError == STAT_LOG_MESSAGE && !(logType_ & STAT_LOG_BE))
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
    if (gStatOutFp != NULL && logType_ & STAT_LOG_BE)
    {
        if (logType_ & STAT_LOG_MRN)
        {
            va_start(args, fmt);
            vsnprintf(msg, BUFSIZE, fmt, args);
            va_end(args);
            mrn_printf(sourceFile, sourceLine, "", gStatOutFp, "%s", msg);
        }
        else
        {
            fprintf(gStatOutFp, "<%s> <%s:%d> ", timeString, sourceFile, sourceLine);
            if (statError != STAT_LOG_MESSAGE && statError != STAT_STDOUT && statError != STAT_VERBOSITY)
            {
                fprintf(gStatOutFp, "%s: STAT returned error type ", localHostName_);
                statPrintErrorType(statError, gStatOutFp);
                fprintf(gStatOutFp, ": ");
            }
            va_start(args, fmt);
            vfprintf(gStatOutFp, fmt, args);
            va_end(args);
            fflush(gStatOutFp);
        }
    }
}

        
void STAT_BackEnd::swDebugBufferToFile()
{
    char fileName[BUFSIZE];

    if (logType_ & STAT_LOG_SWERR)
    {
        if (gStatOutFp != NULL && swDebugFile_ != NULL)
        {
            fclose(swDebugFile_);
            if (gStatOutFp == NULL)
            {
                snprintf(fileName, BUFSIZE, "%s/%s.STATD.log", logOutDir_, localHostName_);
                gStatOutFp = fopen(fileName, "w");
                if (gStatOutFp == NULL)
                {
                    printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: fopen failed for %s\n", strerror(errno), fileName);
                    return;
                }
                if (logType_ & STAT_LOG_MRN)
                    mrn_printf_init(gStatOutFp);
            }
            fwrite(swDebugString_, 1, strlen(swDebugString_), gStatOutFp);
            
            swDebugFile_ = fmemopen(swDebugString_, STAT_SW_DEBUG_BUFFER_LENGTH - 1, "w");
            if (swDebugFile_ == NULL)
            {
                printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "%s: fmemopen failed\n", strerror(errno));
                return;
            }
            Dyninst::Stackwalker::setDebugChannel(swDebugFile_);
            Dyninst::ProcControlAPI::setDebugChannel(swDebugFile_);
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
    int i, found = 0, fLastIVal = -1, address, lineNo, firstLineNo, coNameOffset = -1, coFileNameOffset = -1, obSizeOffset = -1, obSvalOffset = -1, coFirstLineNoOffset = -1, coLnotabOffset = -1, fLastIOffset = -1, pyVarObjectObSizeOffset = -1, pyBytesObjectObSvalOffset = -1;
    long length, pAddr;
    unsigned long long baseAddr, pyCodeObjectBaseAddr, pyUnicodeObjectAddr;
    bool boolRet, isUnicode = false, isPython3 = false;
    char buffer[BUFSIZE], varName[BUFSIZE], exePath[BUFSIZE];
    static map<string, StatPythonCache_t *> pythonCacheMap;
    StatPythonCache_t *pythonCache;
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

    pDebug = static_cast<ProcDebug *>(proc->getProcessState());
    if (pDebug == NULL)
    {
        printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "dynamic_cast returned NULL\n");
        return STAT_STACKWALKER_ERROR;
    }
    snprintf(exePath, BUFSIZE, "%s", pDebug->getExecutablePath().c_str());

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Attempting to get Python frame information for %s\n", exePath);

    /* Check for cached offsets and create a new cache if not found */
    if (pythonCacheMap.find(exePath) == pythonCacheMap.end())
    {
        pythonCache = (StatPythonCache_t *)malloc(sizeof(StatPythonCache_t));
        pythonCache->coNameOffset = -1;
        pythonCache->coFileNameOffset = -1;
        pythonCache->obSizeOffset = -1;
        pythonCache->obSvalOffset = -1;
        pythonCache->coFirstLineNoOffset = -1;
        pythonCache->coLnotabOffset = -1;
        pythonCache->fLastIOffset = -1;
        pythonCache->pyVarObjectObSizeOffset = -1;
        pythonCache->pyBytesObjectObSvalOffset = -1;
        pythonCache->isUnicode = false;
        pythonCache->isPython3 = false;
        pythonCacheMap[exePath] = pythonCache;
    }
    else
        pythonCache = pythonCacheMap[exePath];
    coNameOffset = pythonCache->coNameOffset;
    coFileNameOffset = pythonCache->coFileNameOffset;
    obSizeOffset = pythonCache->obSizeOffset;
    obSvalOffset = pythonCache->obSvalOffset;
    coFirstLineNoOffset = pythonCache->coFirstLineNoOffset;
    coLnotabOffset = pythonCache->coLnotabOffset;
    fLastIOffset = pythonCache->fLastIOffset;
    pyVarObjectObSizeOffset = pythonCache->pyVarObjectObSizeOffset;
    pyBytesObjectObSvalOffset = pythonCache->pyBytesObjectObSvalOffset;
    isUnicode = pythonCache->isUnicode;
    isPython3 = pythonCache->isPython3;

    /* Find all necessary variable field offsets */
    if (coNameOffset == -1 || coFileNameOffset == -1 || obSizeOffset == -1 || obSvalOffset == -1 || coFirstLineNoOffset == -1 || coLnotabOffset == -1 || fLastIOffset == -1)
    {
        boolRet = Symtab::openFile(symtab, pDebug->getExecutablePath().c_str());
        if (boolRet == false)
        {
            printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "openFile returned false for %s\n", pDebug->getExecutablePath().c_str());
            return STAT_STACKWALKER_ERROR;
        }

        /* PyFrameObject used for line number calculation */
        boolRet = symtab->findType(type, "PyFrameObject");
        if (boolRet == false)
        {
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "PyFrameObject not found in %s\n", pDebug->getExecutablePath().c_str());
#ifdef SW_VERSION_8_0_0
            /* Look for libpython* and then search for PyFrameObject in there */
            found = 0;
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

        for (i = 0; i < components->size(); i++)
        {
            field = (*components)[i];
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
        for (i = 0; i < components->size(); i++)
        {
            if (found == 4)
                break;
            field = (*components)[i];
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
        for (i = 0; i < components->size(); i++)
        {
            if (found == 2)
                break;
            field = (*components)[i];
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
        } /* for i */
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

        for (i = 0; i < components->size(); i++)
        {
            field = (*components)[i];
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

        for (i = 0; i < components->size(); i++)
        {
            field = (*components)[i];
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

        for (i = 0; i < components->size(); i++)
        {
            field = (*components)[i];
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

    snprintf(varName, BUFSIZE, "f");
    statError = getVariable(frame, varName, buffer, BUFSIZE);
    if (statError != STAT_OK)
    {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Failed to get Local Variable Value for f\n");
        /* We will at least try to get the file name and return -1 for the line number */
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
        printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to get Local Variable Value for co\n");
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
        for (i = 0; i < length; i = i + 2)
        {
            address += buffer[i];
            if (address > fLastIVal)
                break;
            lineNo += buffer[i + 1];
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
        if (intRet != 0)
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
    int i, intRet, fd, bytesRead, inPort, inParentRank, inRank, mpiRank;
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
    StatError_t statError;

    /* Initialize graphlib if we haven't already done so */
    if (init == 0)
    {
        proctabSize_ = nTasks;
        proctab_ = (MPIR_PROCDESC_EXT *)malloc(proctabSize_*sizeof(MPIR_PROCDESC_EXT));
        proctab_[0].mpirank = myRank_ * nTasks;
        for (i = 0; i < proctabSize_; i++)
        {
            proctab_[i].pd.executable_name = NULL;
            proctab_[i].pd.host_name = NULL;
            proctab_[i].mpirank = proctab_[0].mpirank + i;
        }
        init++;
    }

    /* Delete previously nodes and edges */
    clear3dNodesAndEdges();

    /* Loop around the number of traces to gather */
    for (i = 0; i < nTraces; i++)
    {
        clear2dNodesAndEdges();
        /* Loop around the number of tasks I'm emulating */
        for (j = 0; j < nTasks; j++)
        {
            /* We now create the full eq class set at once */
            if (j >= nEqClasses && nEqClasses != -1)
                break;
            /* Create the trace for this task */
            statError = statBenchCreateTrace(maxDepth, j, nTasks, functionFanout, nEqClasses, i);
        } /* for j nTasks */

        statError = update3dNodesAndEdges();
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Error updating 3d nodes and edges for trace %d of %d\n", i + 1, nTraces);
            return statError;
        }
    } /* for i nTraces */

    return STAT_OK;
}

StatError_t STAT_BackEnd::statBenchCreateTrace(unsigned int maxDepth, unsigned int task, unsigned int nTasks, unsigned int functionFanout, int nEqClasses, unsigned int iteration)
{
    int depth, i, nodeId, prevId, currentTask;
    char temp[8192];
    string path;
    StatBitVectorEdge_t *edge;

    /* set the edge label */
    edge = initializeBitVectorEdge(proctabSize_);
    if (edge == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to initialize edge\n");
        return STAT_ALLOCATE_ERROR;
    }
    if (nEqClasses == -1)
    {
        edge->bitVector[task / STAT_BITVECTOR_BITS] |= STAT_GRAPH_BIT(task % STAT_BITVECTOR_BITS);
    }
    else
    {
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
    }

    path = "/";
    prevId = 0;

    /* Create artificial libc_start_main */
    path += "__libc_start_main";
    nodeId = statStringHash(path.c_str());
    nodes2d_[nodeId] = "__libc_start_main";
    update2dEdge(prevId, nodeId, edge);
    prevId = nodeId;

    /* Create artificial main */
    path += "main";
    nodeId = statStringHash(path.c_str());
    nodes2d_[nodeId] = "main";
    update2dEdge(prevId, nodeId, edge);
    prevId = nodeId;

    /* Seed the random number generator based on my equivalence class */
    if (nEqClasses == -1) /* no limit */
        srand(task + 999999 * iteration);
    else
        srand(task % nEqClasses +  999999 * (1 + iteration));

    /* Generate a trace and add it to the graph */
    depth = rand() % maxDepth;
    for (i = 0; i < depth; i++)
    {
        snprintf(temp, 8192, "depth%dfun%d", i, rand() % functionFanout);
        path += temp;
        nodeId = statStringHash(path.c_str());
        nodes2d_[nodeId] = temp;
        update2dEdge(prevId, nodeId, edge);
        prevId = nodeId;
    }

    statFreeEdge(edge);

    return STAT_OK;
}


