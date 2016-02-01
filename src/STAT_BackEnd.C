/*
Copyright (c) 2007-2014, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory
Written by Gregory Lee [lee218@llnl.gov], Dorian Arnold, Matthew LeGendre, Dong Ahn, Bronis de Supinski, Barton Miller, and Martin Schulz.
LLNL-CODE-624152.
All rights reserved.

This file is part of STAT. For details, see http://www.paradyn.org/STAT/STAT.html. Please also read STAT/LICENSE.

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

//! the STAT log handle, global for filter access
FILE *gStatOutFp = NULL;

//! a pointer to the current STAT_BackEnd object
STAT_BackEnd *gBePtr;

extern const char *gNodeAttrs[];
extern const char *gEdgeAttrs[];
extern int gNumNodeAttrs;
extern int gNumEdgeAttrs;

StatError_t statInit(int *argc, char ***argv, StatDaemonLaunch_t launchType)
{
    int intRet;
    sigset_t mask;
    lmon_rc_e lmonRet;

    /* unblock all signals. */
    /* In 3/2014, SIGUSR2 was found to be blocked on Cray systems, which was causing detach to hang */
    intRet = sigfillset(&mask);
    if (intRet == 0)
    {
        intRet = pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
        if (intRet != 0)
            fprintf(stderr, "warning: failed to set pthread_sigmask: %s\n", strerror(errno));
    }

    if (launchType == STATD_LMON_LAUNCH)
    {
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
        lmonRet = LMON_be_finalize();
        if (lmonRet != LMON_OK)
        {
            fprintf(stderr, "Failed to finalize Launchmon\n");
            return STAT_LMON_ERROR;
        }
    }

    return STAT_OK;
}

STAT_BackEnd::STAT_BackEnd(StatDaemonLaunch_t launchType) :
    launchType_(launchType),
    swLogBuffer_(STAT_SW_DEBUG_BUFFER_LENGTH)
{
    gStatOutFp = NULL;
    proctabSize_ = 0;
    processMapNonNull_ = 0;
    logType_ = 0;
    parentHostName_ = NULL;
    swDebugFile_ = NULL;
#ifdef BGL
    errOutFp_ = stdout;
#else
    errOutFp_ = stderr;
#endif
    initialized_ = false;
    connected_ = false;
    isRunning_ = false;
#ifdef GROUP_OPS
    doGroupOps_ = false;
#endif
    network_ = NULL;
    myRank_ = 0;
    parentRank_ = 0;
    parentPort_ = 0;
    broadcastStream_ = NULL;
    proctab_ = NULL;
    sampleType_ = 0;
    nVariables_ = 0;
    extractVariables_ = NULL;
    snprintf(outDir_, BUFSIZE, "NULL");
    snprintf(filePrefix_, BUFSIZE, "NULL");
#ifdef STAT_FGFS
    fgfsCommFabric_ = NULL;
#endif
#ifdef DYSECTAPI
    dysectBE_ = NULL;
#endif
    gBePtr = this;
	registerSignalHandlers(true);
#ifdef GRAPHLIB_3_0
    threadBvLength_ = STAT_BITVECTOR_BITS; // for now we restict to 64 threads per process
#endif
}

STAT_BackEnd::~STAT_BackEnd()
{
    int i;
    graphlib_error_t graphlibError;
    map<int, StatBitVectorEdge_t *>::iterator nodeInEdgeMapIter;

    clear2dNodesAndEdges();
    clear3dNodesAndEdges();

    if (parentHostName_ != NULL)
    {
        free(parentHostName_);
        parentHostName_ = NULL;
    }

    if (extractVariables_ != NULL)
    {
        free(extractVariables_);
        extractVariables_ = NULL;
    }

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

    graphlibError = graphlib_Finish();
    if (GRL_IS_FATALERROR(graphlibError))
        fprintf(errOutFp_, "Warning: Failed to finish graphlib\n");

    statFreeReorderFunctions();
    statFreeBitVectorFunctions();
    statFreeCountRepFunctions();
    statFreeMergeFunctions();

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

#ifdef DYSECTAPI
    dysectBE_ = NULL;
#endif

	registerSignalHandlers(false);
    gBePtr = NULL;
}

void STAT_BackEnd::clear2dNodesAndEdges()
{
    map<int, pair<int, StatBitVectorEdge_t *> >::iterator edgesIter;

    nodes2d_.clear();
    for (edgesIter = edges2d_.begin(); edgesIter != edges2d_.end(); edgesIter++)
        if (edgesIter->second.second != NULL)
            statFreeEdge(edgesIter->second.second);
    edges2d_.clear();
#ifdef GRAPHLIB_3_0
    nodeIdToAttrs_.clear();
    map<int, map<string, void *> >::iterator edgeIdToAttrsIter;
    map<string, void *>::iterator edgeAttrsIter;
    for (edgeIdToAttrsIter = edgeIdToAttrs_.begin(); edgeIdToAttrsIter != edgeIdToAttrs_.end(); edgeIdToAttrsIter++)
        for (edgeAttrsIter = edgeIdToAttrsIter->second.begin(); edgeAttrsIter != edgeIdToAttrsIter->second.end(); edgeAttrsIter++)
            statFreeEdgeAttr(edgeAttrsIter->first.c_str(), edgeAttrsIter->second);
    edgeIdToAttrs_.clear();
#endif
}

void STAT_BackEnd::clear3dNodesAndEdges()
{
    map<int, pair<int, StatBitVectorEdge_t *> >::iterator edgesIter;

    nodes3d_.clear();
    for (edgesIter = edges3d_.begin(); edgesIter != edges3d_.end(); edgesIter++)
        if (edgesIter->second.second != NULL)
            statFreeEdge(edgesIter->second.second);
    edges3d_.clear();

#ifdef GRAPHLIB_3_0
    nodeIdToAttrs_.clear();
    map<int, map<string, void *> >::iterator edgeIdToAttrsIter;
    map<string, void *>::iterator edgeAttrsIter;
    for (edgeIdToAttrsIter = edgeIdToAttrs3d_.begin(); edgeIdToAttrsIter != edgeIdToAttrs3d_.end(); edgeIdToAttrsIter++)
        for (edgeAttrsIter = edgeIdToAttrsIter->second.begin(); edgeAttrsIter != edgeIdToAttrsIter->second.end(); edgeAttrsIter++)
            statFreeEdgeAttr(edgeAttrsIter->first.c_str(), edgeAttrsIter->second);
    edgeIdToAttrs3d_.clear();
#endif
}


StatError_t STAT_BackEnd::update3dNodesAndEdges()
{
    map<int, string>::iterator nodesIter;
    map<int, pair<int, StatBitVectorEdge_t *> >::iterator edgesIter;
    StatBitVectorEdge_t *edge;

    for (nodesIter = nodes2d_.begin(); nodesIter != nodes2d_.end(); nodesIter++)
        nodes3d_[nodesIter->first] = nodesIter->second;

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

#ifdef GRAPHLIB_3_0
        int i, dst;
        StatCountRepEdge_t *countRepEdge = NULL;
        dst = edgesIter->first;
        if (edgeIdToAttrs3d_.find(dst) == edgeIdToAttrs3d_.end())
        {
            map<string, void *> edgeAttrs;
            for (i = 0; i < gNumEdgeAttrs; i++)
                edgeAttrs[gEdgeAttrs[i]] = NULL;
            edgeIdToAttrs3d_[edgesIter->first] = edgeAttrs;
        }
        for (i = 0; i < gNumEdgeAttrs; i++)
            edgeIdToAttrs3d_[dst][gEdgeAttrs[i]] = statMergeEdgeAttr(gEdgeAttrs[i], edgeIdToAttrs3d_[dst][gEdgeAttrs[i]], edgeIdToAttrs_[dst][gEdgeAttrs[i]]);
        if (sampleType_ & STAT_SAMPLE_COUNT_REP)
        {
            countRepEdge = getBitVectorCountRep((StatBitVectorEdge_t *)edgeIdToAttrs3d_[dst]["bv"], statRelativeRankToAbsoluteRank);
            if (countRepEdge == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to translate bit vector into count + representative\n");
                return STAT_ALLOCATE_ERROR;
            }
            statFreeEdgeAttr("count", edgeIdToAttrs3d_[dst]["count"]);
            statFreeEdgeAttr("rep", edgeIdToAttrs3d_[dst]["rep"]);
            edgeIdToAttrs3d_[dst]["count"] = statCopyEdgeAttr("count", (void *)&countRepEdge->count);
            edgeIdToAttrs3d_[dst]["rep"] = statCopyEdgeAttr("rep" , (void *)&countRepEdge->representative);
            edgeIdToAttrs_[dst]["sum"] = statMergeEdgeAttr("sum", edgeIdToAttrs_[dst]["sum"], (void *)&countRepEdge->checksum);
            statFreeCountRepEdge(countRepEdge);
        }
        if (sampleType_ & STAT_SAMPLE_THREADS)
        {
            countRepEdge = getBitVectorCountRep((StatBitVectorEdge_t *)edgeIdToAttrs3d_[dst]["tbv"], statDummyRelativeRankToAbsoluteRank);
            if (countRepEdge == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to translate bit vector into count + representative\n");
                return STAT_ALLOCATE_ERROR;
            }
            statFreeEdgeAttr("tcount", edgeIdToAttrs3d_[dst]["tcount"]);
            edgeIdToAttrs3d_[dst]["tcount"] = statCopyEdgeAttr("tcount", (void *)&countRepEdge->count);
            statFreeEdgeAttr("sum", edgeIdToAttrs3d_[dst]["sum"]);
            edgeIdToAttrs3d_[dst]["sum"] = statCopyEdgeAttr("sum", (void *)&countRepEdge->count);
            statFreeEdgeAttr("tbvsum", edgeIdToAttrs3d_[dst]["tbvsum"]);
            // we need a unique checksum for threads across damons:
            int64_t tbvsum = countRepEdge->checksum * (myRank_ + 1);
            edgeIdToAttrs3d_[dst]["tbvsum"] = statCopyEdgeAttr("tbvsum", (void *)&tbvsum);
            statFreeCountRepEdge(countRepEdge);
        }
#endif

    } // for edgesIter

    return STAT_OK;
}


StatError_t STAT_BackEnd::update2dEdge(int src, int dst, StatBitVectorEdge_t *edge, Dyninst::THR_ID tid)
{
    StatBitVectorEdge_t *newEdge = NULL;

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

#ifdef GRAPHLIB_3_0
    int i, tidIndex;
    map<string, void *> edgeAttrs;
    StatCountRepEdge_t *countRepEdge = NULL;
    if (edgeIdToAttrs_.find(dst) == edgeIdToAttrs_.end())
    {
        for (i = 0; i < gNumEdgeAttrs; i++)
            edgeAttrs[gEdgeAttrs[i]] = NULL;
        edgeIdToAttrs_[dst] = edgeAttrs;
    }

    // we always maintain a bit vector and just won't send it if sampling count + rep
    statFreeEdgeAttr("bv", edgeIdToAttrs_[dst]["bv"]);
    edgeIdToAttrs_[dst]["bv"] = statCopyEdgeAttr("bv", (void *)edges2d_[dst].second);
    if (sampleType_ & STAT_SAMPLE_THREADS)
    {
        newEdge = initializeBitVectorEdge(STAT_BITVECTOR_BITS);
        if (newEdge == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to initialize newEdge\n");
            return STAT_ALLOCATE_ERROR;
        }
        tidIndex = find(threadIds_.begin(), threadIds_.end(), tid) - threadIds_.begin();
        tidIndex = tidIndex % threadBvLength_;
        newEdge->bitVector[tidIndex / STAT_BITVECTOR_BITS] |= STAT_GRAPH_BIT(tidIndex % STAT_BITVECTOR_BITS);
        edgeIdToAttrs_[dst]["tbv"] = statMergeEdgeAttr("tbv", edgeIdToAttrs_[dst]["tbv"], (void *)newEdge);
        statFreeEdge(newEdge);

        countRepEdge = getBitVectorCountRep((StatBitVectorEdge_t *)edgeIdToAttrs_[dst]["tbv"], statDummyRelativeRankToAbsoluteRank);
        if (countRepEdge == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to translate bit vector into count + representative\n");
            return STAT_ALLOCATE_ERROR;
        }
        statFreeEdgeAttr("tcount", edgeIdToAttrs_[dst]["tcount"]);
        edgeIdToAttrs_[dst]["tcount"] = statCopyEdgeAttr("tcount", (void *)&countRepEdge->count);
        edgeIdToAttrs_[dst]["sum"] = statMergeEdgeAttr("sum", edgeIdToAttrs_[dst]["sum"], (void *)&countRepEdge->count);
        // we need a unique checksum for threads across damons:
        int64_t tbvsum = countRepEdge->checksum * (myRank_ + 1);
        edgeIdToAttrs_[dst]["tbvsum"] = statMergeEdgeAttr("tbvsum", edgeIdToAttrs_[dst]["tbvsum"], (void *)&tbvsum);

        statFreeCountRepEdge(countRepEdge);
    } //if (sampleType_ & STAT_SAMPLE_THREADS)

    if (sampleType_ & STAT_SAMPLE_COUNT_REP)
    {
        countRepEdge = getBitVectorCountRep(edges2d_[dst].second, statRelativeRankToAbsoluteRank);
        if (countRepEdge == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to translate bit vector into count + representative\n");
            return STAT_ALLOCATE_ERROR;
        }
        statFreeEdgeAttr("count", edgeIdToAttrs_[dst]["count"]);
        statFreeEdgeAttr("rep", edgeIdToAttrs_[dst]["rep"]);
        edgeIdToAttrs_[dst]["count"] = statCopyEdgeAttr("count", (void *)&countRepEdge->count);
        edgeIdToAttrs_[dst]["rep"] = statCopyEdgeAttr("rep" , (void *)&countRepEdge->representative);
        edgeIdToAttrs_[dst]["sum"] = statMergeEdgeAttr("sum", edgeIdToAttrs_[dst]["sum"], (void *)&countRepEdge->checksum);
        statFreeCountRepEdge(countRepEdge);
    }
#endif

    return STAT_OK;
}

StatError_t STAT_BackEnd::generateGraphs(graphlib_graph_p *prefixTree2d, graphlib_graph_p *prefixTree3d)
{
    int i;
    string::size_type delimPos;
    string tempString;
    StatCountRepEdge_t *countRepEdge = NULL;
    graphlib_graph_p *currentGraph;
    graphlib_error_t graphlibError;
#ifdef GRAPHLIB_3_0
    graphlib_nodeattr_t nodeAttr = {1,0,20,GRC_LIGHTGREY,0,0,(char *)"",-1,NULL};
    graphlib_edgeattr_t edgeAttr = {1,0,NULL,0,0,0,NULL};
#else
    graphlib_nodeattr_t nodeAttr = {1,0,20,GRC_LIGHTGREY,0,0,(char *)"",-1};
    graphlib_edgeattr_t edgeAttr = {1,0,NULL,0,0,0};
#endif
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

        *currentGraph = createRootedGraph(sampleType_);
        if (*currentGraph == NULL)
        {
            printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error initializing graph\n");
            return STAT_GRAPHLIB_ERROR;
        }
        for (nodesIter = (*nodes).begin(); nodesIter != (*nodes).end(); nodesIter++)
        {

#ifdef GRAPHLIB_3_0
            int index;
            map<string, string>::iterator nodeAttrIter;
            nodeAttr.attr_values = (void **)calloc(1, gNumNodeAttrs * sizeof(void *));
            if (nodeAttr.attr_values == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Error callocing %d nodeAttr.attr_values\n", strerror(errno), gNumNodeAttrs);
                return STAT_ALLOCATE_ERROR;
            }
            for (nodeAttrIter = nodeIdToAttrs_[nodesIter->first].begin(); nodeAttrIter != nodeIdToAttrs_[nodesIter->first].end(); nodeAttrIter++)
            {
                graphlibError = graphlib_getNodeAttrIndex(*currentGraph, nodeAttrIter->first.c_str(), &index);
                if (GRL_IS_FATALERROR(graphlibError))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to get node attr index for key %s\n", nodeAttrIter->first.c_str());
                    return STAT_GRAPHLIB_ERROR;
                }
                nodeAttr.attr_values[index] =  statCopyNodeAttr(nodeAttrIter->first.c_str(), nodeAttrIter->second.c_str());
            }
#else
            /* Add \ character before '<' and '>' characters for dot format */
            tempString = nodesIter->second;
            delimPos = -2;
            while (1)
            {
                delimPos = tempString.find('<', delimPos + 2);
                if (delimPos == string::npos)
                    break;
                tempString.insert(delimPos, "\\");
            }
            delimPos = -2;
            while (1)
            {
                delimPos = tempString.find('>', delimPos + 2);
                if (delimPos == string::npos)
                    break;
                tempString.insert(delimPos, "\\");
            }
            nodeAttr.label = (void *)tempString.c_str();
#endif
            graphlibError = graphlib_addNode(*currentGraph, nodesIter->first, &nodeAttr);
            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error adding node to graph\n");
                return STAT_GRAPHLIB_ERROR;
            }
#ifdef GRAPHLIB_3_0
            for (int j = 0; j < gNumNodeAttrs; j++)
            {
                char *nodeAttrKey;
                graphlibError = graphlib_getNodeAttrKey(*currentGraph, j, &nodeAttrKey);
                if (GRL_IS_FATALERROR(graphlibError))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error getting key for attribute %d\n", j);
                    return STAT_GRAPHLIB_ERROR;
                }
                statFreeNodeAttr(nodeAttrKey, nodeAttr.attr_values[j]);
            }
            free(nodeAttr.attr_values);
#endif
        }
        for (edgesIter = (*edges).begin(); edgesIter != (*edges).end(); edgesIter++)
        {
#ifdef GRAPHLIB_3_0
            int index;
            edgeAttr.attr_values = (void **)calloc(1, gNumEdgeAttrs * sizeof(void *));
            if (edgeAttr.attr_values == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Error callocing %d edgeAttr.attr_values\n", strerror(errno), gNumEdgeAttrs);
                return STAT_ALLOCATE_ERROR;
            }
            map<string, void *>::iterator edgeAttrIter, edgeAttrIterEnd;
            if (currentGraph == prefixTree2d)
            {
                edgeAttrIter = edgeIdToAttrs_[edgesIter->first].begin();
                edgeAttrIterEnd = edgeIdToAttrs_[edgesIter->first].end();
            }
            else
            {
                edgeAttrIter = edgeIdToAttrs3d_[edgesIter->first].begin();
                edgeAttrIterEnd = edgeIdToAttrs3d_[edgesIter->first].end();
            }
            while (edgeAttrIter != edgeAttrIterEnd)
            {
                graphlibError = graphlib_getEdgeAttrIndex(*currentGraph, edgeAttrIter->first.c_str(), &index);
                if (GRL_IS_FATALERROR(graphlibError))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to get edge attr index for key %s\n", edgeAttrIter->first.c_str());
                    return STAT_GRAPHLIB_ERROR;
                }
                if ((sampleType_ & STAT_SAMPLE_COUNT_REP && strcmp(edgeAttrIter->first.c_str(), "bv") == 0) || strcmp(edgeAttrIter->first.c_str(), "tbv") == 0)
                {
                    edgeAttrIter++;
                    continue;
                }
                edgeAttr.attr_values[index] = statCopyEdgeAttr(edgeAttrIter->first.c_str(), edgeAttrIter->second);
                edgeAttrIter++;
            }
#else
            if (sampleType_ & STAT_SAMPLE_COUNT_REP)
            {
                countRepEdge = getBitVectorCountRep(edgesIter->second.second, statRelativeRankToAbsoluteRank);
                if (countRepEdge == NULL)
                {
                    printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to translate bit vector into count + representative\n");
                    return STAT_ALLOCATE_ERROR;
                }
                edgeAttr.label = (void *)countRepEdge;
            }
            else
                edgeAttr.label = (void *)edgesIter->second.second;
#endif

            graphlibError = graphlib_addDirectedEdge(*currentGraph, edgesIter->second.first, edgesIter->first, &edgeAttr);
            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Error adding edge to graph\n");
                return STAT_GRAPHLIB_ERROR;
            }
#ifdef GRAPHLIB_3_0
            for (int j = 0; j < gNumEdgeAttrs; j++)
            {
                char *edgeAttrKey;
                graphlibError = graphlib_getEdgeAttrKey(*currentGraph, j, &edgeAttrKey);
                if (GRL_IS_FATALERROR(graphlibError))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to get edge attr key for index %d\n", j);
                    return STAT_GRAPHLIB_ERROR;
                }
                statFreeEdgeAttr(edgeAttrKey, edgeAttr.attr_values[j]);
            }
            free(edgeAttr.attr_values);
#else
            if (sampleType_ & STAT_SAMPLE_COUNT_REP)
            {
                graphlibError = graphlib_delEdgeAttr(edgeAttr, statFreeCountRepEdge);
                if (GRL_IS_FATALERROR(graphlibError))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to free edge attr\n");
                    return STAT_GRAPHLIB_ERROR;
                }
            }
#endif

        }
    }

    return STAT_OK;
}


StatError_t STAT_BackEnd::init()
{
    int intRet;
    char abuf[INET_ADDRSTRLEN], fileName[BUFSIZE], *envValue;
    FILE *file;
    struct sockaddr_in *sinp = NULL;
    struct addrinfo *addinf = NULL;
    graphlib_error_t graphlibError;

    /* Get hostname and IP address */
    intRet = gethostname(localHostName_, BUFSIZE);
    if (intRet != 0)
    {
        fprintf(errOutFp_, "Error: gethostname returned %d\n", intRet);
        return STAT_SYSTEM_ERROR;
    }

    intRet = getaddrinfo(localHostName_, NULL, NULL, &addinf);
    if (intRet != 0)
        fprintf(errOutFp_, "Warning: getaddrinfo returned %d\n", intRet);
    if (addinf != NULL)
        sinp = (struct sockaddr_in *)addinf->ai_addr;
    if (sinp != NULL)
        snprintf(localIp_, BUFSIZE, "%s", inet_ntop(AF_INET, &sinp->sin_addr, abuf, INET_ADDRSTRLEN));
    if (addinf != NULL)
        freeaddrinfo(addinf);

    graphlibError = graphlib_Init();
    if (GRL_IS_FATALERROR(graphlibError))
    {
        fprintf(errOutFp_, "Failed to initialize Graphlib\n");
        return STAT_GRAPHLIB_ERROR;
    }
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
    {
        intRet = setenv("MRNET_DEBUG_LOG_DIRECTORY", envValue, 1);
        if (intRet != 0)
            fprintf(errOutFp_, "Warning: %s: setenv returned %d\n", strerror(errno), intRet);
    }

    /* Redirect output to a file */
    envValue = getenv("STAT_OUTPUT_REDIRECT_DIR");
    if (envValue != NULL)
    {
        snprintf(fileName, BUFSIZE, "%s/%s.stdout.txt", envValue, localHostName_);
        file = freopen(fileName, "w", stdout);
        if (file == NULL)
            fprintf(errOutFp_, "Warning: %s: frepoen returned NULL for stdout to file %s\n", strerror(errno), fileName);
        snprintf(fileName, BUFSIZE, "%s/%s.stderr.txt", envValue, localHostName_);
        file = freopen(fileName, "w", stderr);
        if (file == NULL)
            fprintf(errOutFp_, "Warning: %s: frepoen returned NULL for stdwerr to file %s\n", strerror(errno), fileName);
    }

#ifdef BGL
    doGroupOps_ = true;
#else
    envValue = getenv("STAT_GROUP_OPS");
    doGroupOps_ = (envValue != NULL);
#endif

    return STAT_OK;
}


StatError_t STAT_BackEnd::initLmon()
{
    int size;
    lmon_rc_e lmonRet;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Registering unpack function with Launchmon\n");
    lmonRet = LMON_be_regUnpackForFeToBe(unpackStatBeInfo);
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to register unpack function\n");
        return STAT_LMON_ERROR;
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Launchmon handshaking\n");
    lmonRet = LMON_be_handshake(NULL);
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to handshake with Launchmon\n");
        return STAT_LMON_ERROR;
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Waiting for launchmon to be ready\n");
    lmonRet = LMON_be_ready(NULL);
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Launchmon failed to get ready\n");
        return STAT_LMON_ERROR;
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Allocating process table\n");
    lmonRet = LMON_be_getMyProctabSize(&size);
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to get Process Table Size\n");
        return STAT_LMON_ERROR;
    }
    proctab_ = (MPIR_PROCDESC_EXT *)malloc(size * sizeof(MPIR_PROCDESC_EXT));
    if (proctab_ == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Error allocating process table\n", strerror(errno));
        return STAT_ALLOCATE_ERROR;
    }

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Getting my process table entries\n");
    lmonRet = LMON_be_getMyProctab(proctab_, &proctabSize_, size);
    if (lmonRet != LMON_OK)
    {
        printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Failed to get Process Table\n");
        return STAT_LMON_ERROR;
    }

    initialized_ = true;
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Launchmon successfully initialized\n");

    return STAT_OK;
}


static void onCrashWrap(int sig, siginfo_t *info, void *context)
{
    gBePtr->onCrash(sig, info, context);
}


void STAT_BackEnd::onCrash(int sig, siginfo_t *, void *context)
{
    int addrListSize;
    unsigned int j;
    static const unsigned int maxStackSize = 256;
    char **namedSw, **i;
    void *stackSize[maxStackSize];
    StatError_t statError;
    graphlib_error_t graphlibError;
    graphlib_graph_p prefixTree2d = NULL, prefixTree3d = NULL;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "STATD intercepted signal %d\n", sig);
    registerSignalHandlers(false);

    if (swDebugFile_)
    {
        addrListSize = backtrace(stackSize, maxStackSize);
        namedSw = backtrace_symbols(stackSize, addrListSize);
        if (namedSw)
        {
            fprintf(swDebugFile_, "Stacktrace upon signal %d:\n", sig);
            for (i = namedSw, j = 0; j < addrListSize; i++, j++)
            {
                if (namedSw[j])
                  	fprintf(swDebugFile_, "%p - %s\n", stackSize[j], namedSw[j]);
            }
            fflush(swDebugFile_);
        }
        swDebugBufferToFile();
    }

    if (strcmp(outDir_, "NULL") != 0 && strcmp(filePrefix_, "NULL") != 0)
    {
        statError = generateGraphs(&prefixTree2d, &prefixTree3d);
        if (statError == STAT_OK)
        {
            extern int gStatGraphRoutinesTotalWidth;
            char outFile[BUFSIZE];

            gStatGraphRoutinesTotalWidth = statBitVectorLength(proctabSize_);

            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Exporting 2D graph to dot\n");
            graphlibError = graphlib_colorGraphByLeadingEdgeLabel(prefixTree2d);
            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "graphlib error coloring graph by leading edge label\n");
                if (sig != SIGTERM)
                    abort();
                else
                    exit(sig);
            }
            graphlibError = graphlib_scaleNodeWidth(prefixTree2d, 80, 160);
            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "graphlib error scaling node width\n");
                if (sig != SIGTERM)
                    abort();
                else
                    exit(sig);
            }
            snprintf(outFile, BUFSIZE, "%s/%s.BE_%s_%d.dot", outDir_, filePrefix_, localHostName_, myRank_);
            graphlibError = graphlib_exportGraph(outFile, GRF_DOT, prefixTree2d);
            if (GRL_IS_FATALERROR(graphlibError))
            {
                printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "graphlib error exporting graph to dot format\n");
                if (sig != SIGTERM)
                    abort();
                else
                    exit(sig);
            }
        }
    }

    if (sig != SIGTERM)
        abort();
    else
        exit(sig);
}


void STAT_BackEnd::registerSignalHandlers(bool enable)
{
    struct sigaction action;

    bzero(&action, sizeof(struct sigaction));
    if (!enable)
    {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "de-registering signal handlers\n");
        action.sa_handler = SIG_DFL;
    }
    else
    {
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "registering signal handlers\n");
        action.sa_sigaction = onCrashWrap;
        action.sa_flags = SA_SIGINFO;
    }

    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGBUS, &action, NULL);
    sigaction(SIGABRT, &action, NULL);
    sigaction(SIGILL, &action, NULL);
    sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}


StatError_t STAT_BackEnd::addSerialProcess(const char *pidString)
{
    static int rank = -1;
    unsigned int pid;
    char *remoteNode = NULL, *executablePath = NULL;
    string::size_type delimPos;
    string remotePid = pidString, remoteHost;

    rank++;
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Adding serial process %s with rank %d\n", pidString, rank);

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

    if (strcmp(remoteHost.c_str(), "localhost") == 0 || strcmp(remoteHost.c_str(), localHostName_) == 0 || strcmp(remoteHost.c_str(), localIp_) == 0)
    {
        proctabSize_++;
        proctab_ = (MPIR_PROCDESC_EXT *)realloc(proctab_, proctabSize_ * sizeof(MPIR_PROCDESC_EXT));
        if (proctab_ == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to allocate memory for the process table\n", strerror(errno));
            return STAT_ALLOCATE_ERROR;
        }

        proctab_[proctabSize_ - 1].mpirank = rank;
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
        if (initialized_ == false)
        {
            printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "Trying to connect when not initialized\n");
            return STAT_LMON_ERROR;
        }

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

#ifdef DYSECTAPI
DysectAPI::BE *STAT_BackEnd::getDysectBe()
{
    return dysectBE_;
}
#endif

StatError_t STAT_BackEnd::mainLoop()
{
    int tag = 0, ackTag, intRet, nEqClasses, swNotificationFd, mrnNotificationFd, maxFd, nodeId, bitVectorLength, stopArrayLen, majorVersion, minorVersion, revisionVersion;
    unsigned int nTraces, traceFrequency, nTasks, functionFanout, maxDepth, nRetries, retryFrequency, *stopArray = NULL, previousSampleType = 0;
    uint64_t byteArrayLen = 0;
    char *byteArray = NULL, *variableSpecification = NULL;
    bool boolRet, recvShouldBlock;
    Stream *stream = NULL;
    fd_set readFds, writeFds, exceptFds;
    PacketPtr packet;
    StatError_t statError;
    graphlib_error_t graphlibError;
    graphlib_graph_p prefixTree2d = NULL, prefixTree3d = NULL;
    StatBitVectorEdge_t *edge;
    map<int, Walker *>::iterator processMapIter;
    ProcDebug *pDebug = NULL;
#ifdef STAT_FGFS
    MRNetSymbolReaderFactory *msrf = NULL;
#endif

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
sleep(2);            
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

                /* Check if target processes exited, if so, set Walker to NULL */
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
        /* TODO blocking here breaks Dysect (hangs) */
        //recvShouldBlock = (swNotificationFd == -1);
        recvShouldBlock=false;
        intRet = network_->recv(&tag, packet, &stream, recvShouldBlock);
        if (intRet == 0)
        {
#ifdef DYSECTAPI
            if (dysectBE_)
            {
                DysectAPI::DysectErrorCode dysectRet = dysectBE_->handleAll();
                if (dysectRet != DysectAPI::OK)
                {
                    printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "failure on call to dysectBE_ hanldeAll\n");
                    return STAT_STACKWALKER_ERROR;
                }
            }
#endif
            continue;
        }
        else if (intRet != 1)
        {
            printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::recv() failure %d\n", intRet);
            return STAT_MRNET_ERROR;
        }
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Packet received with tag %d (first = %d)\n", tag, FirstApplicationTag);

        /* Initialize return value to error code */
        intRet = 1;
        ackTag = -1;

        switch(tag)
        {
            case PROT_ATTACH_APPLICATION:
                char *in1, *in2;
                if (packet->unpack("%s %s", &in1, &in2) != -1)
                {
                    snprintf(outDir_, BUFSIZE, in1);
                    snprintf(filePrefix_, BUFSIZE, in2);
                    free(in1);
                    free(in2);
                }
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Attach request received for %s %s\n", outDir_, filePrefix_);
                statError = attach();
                if (statError == STAT_OK)
                    intRet = 0;
                ackTag = PROT_ATTACH_APPLICATION_RESP;
                break;

            case PROT_PAUSE_APPLICATION:
                statError = pause();
                if (statError == STAT_OK)
                    intRet = 0;
                ackTag = PROT_PAUSE_APPLICATION_RESP;
                break;

            case PROT_RESUME_APPLICATION:
                statError = resume();
                if (statError == STAT_OK)
                    intRet = 0;
                ackTag = PROT_RESUME_APPLICATION_RESP;
                break;

            case PROT_SAMPLE_TRACES:
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

                /* When switching between count + rep and full list, we need to clear previous samples */
                if ((previousSampleType ^ sampleType_) & STAT_SAMPLE_COUNT_REP)
                    sampleType_ |= STAT_SAMPLE_CLEAR_ON_SAMPLE;
                previousSampleType = sampleType_;

                statError = sampleStackTraces(nTraces, traceFrequency, nRetries, retryFrequency, variableSpecification);
                if (statError == STAT_OK)
                    intRet = 0;
                statError = generateGraphs(&prefixTree2d, &prefixTree3d);
                if (statError != STAT_OK)
                    intRet = 1;
                ackTag = PROT_SAMPLE_TRACES_RESP;
                free(variableSpecification);
                variableSpecification = NULL;
                break;

            case PROT_SEND_LAST_TRACE:
                /* Free previously allocated byte array */
                if (byteArray != NULL)
                {
                    free(byteArray);
                    byteArray = NULL;
                }

                graphlibError = graphlib_serializeBasicGraph(prefixTree2d, &byteArray, &byteArrayLen);
                if (GRL_IS_FATALERROR(graphlibError))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to serialize 2D prefix tree\n");
                    return STAT_GRAPHLIB_ERROR;
                }
                ackTag = PROT_SEND_LAST_TRACE_RESP;

#ifdef DYSECTAPI
                if(dysectBE_)
                    dysectBE_->setReturnControlToDysect(true);
#endif
                break;

            case PROT_SEND_TRACES:
                /* Free previously allocated byte array */
                if (byteArray != NULL)
                {
                    free(byteArray);
                    byteArray = NULL;
                }

                graphlibError = graphlib_serializeBasicGraph(prefixTree3d, &byteArray, &byteArrayLen);
                if (GRL_IS_FATALERROR(graphlibError))
                {
                    printMsg(STAT_GRAPHLIB_ERROR, __FILE__, __LINE__, "Failed to serialize 3D prefix tree\n");
                    return STAT_GRAPHLIB_ERROR;
                }
                ackTag = PROT_SEND_TRACES_RESP;

#ifdef DYSECTAPI
                if(dysectBE_)
                    dysectBE_->setReturnControlToDysect(true);
#endif
                break;

            case PROT_SEND_NODE_IN_EDGE:
                if (packet->unpack("%d", &nodeId) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_SEND_NODE_IN_EDGE) failed\n");
                    if (sendAck(stream, PROT_SEND_NODE_IN_EDGE_RESP, intRet) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_SEND_NODE_IN_EDGE_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }
                    return STAT_MRNET_ERROR;
                }

                /* Free previously allocated byte array */
                if (byteArray != NULL)
                {
                    free(byteArray);
                    byteArray = NULL;
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
                byteArrayLen = statSerializeEdgeLength(edge);
                byteArray = (char *)malloc(byteArrayLen);
                if (byteArray == NULL)
                {
                    printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to allocate %lu bytes for byteArray\n", byteArrayLen);
                    if (edge != NULL && edges2d_.find(nodeId) == edges2d_.end())
                        free(edge);
                    return STAT_ALLOCATE_ERROR;
                }
                statSerializeEdge(byteArray, edge);
                if (edge != NULL && edges2d_.find(nodeId) == edges2d_.end())
                    statFreeEdge(edge);
                edge = NULL;
                ackTag = PROT_SEND_NODE_IN_EDGE_RESP;
                break;

            case PROT_DETACH_APPLICATION:
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

                statError = detach(stopArray, stopArrayLen);
                if (statError ==  STAT_OK)
                    intRet = 0;
                if (stopArray != NULL)
                    free(stopArray);
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Detaching complete, sending ackowledgement\n");
                ackTag = PROT_DETACH_APPLICATION_RESP;
                break;

            case PROT_TERMINATE_APPLICATION:
                statError = terminate();
                if (statError ==  STAT_OK)
                    intRet = 0;
                ackTag = PROT_TERMINATE_APPLICATION_RESP;
                break;

            case PROT_STATBENCH_CREATE_TRACES:
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

                statError = statBenchCreateTraces(maxDepth, nTasks, nTraces, functionFanout, nEqClasses);
                if (statError == STAT_OK)
                    intRet = 0;
                statError = generateGraphs(&prefixTree2d, &prefixTree3d);
                if (statError != STAT_OK)
                    intRet = 1;
                ackTag = PROT_STATBENCH_CREATE_TRACES_RESP;
                break;

            case PROT_CHECK_VERSION:
                if (packet->unpack("%d %d %d", &majorVersion, &minorVersion, &revisionVersion) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_CHECK_VERSION) failed\n");
                    if (sendAck(stream, PROT_CHECK_VERSION_RESP, intRet) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(PROT_CHECK_VERSION_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }
                    return STAT_MRNET_ERROR;
                }
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to compare my version %d.%d.%d to the FE version %d.%d.%d\n", STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION, majorVersion, minorVersion, revisionVersion);
                if (majorVersion == STAT_MAJOR_VERSION && minorVersion == STAT_MINOR_VERSION && revisionVersion == STAT_REVISION_VERSION)
                    intRet = 0;
                if (intRet != 0)
                    printMsg(STAT_VERSION_ERROR, __FILE__, __LINE__, "Daemon reports version mismatch: FE = %d.%d.%d, Daemon = %d.%d.%d\n", majorVersion, minorVersion, revisionVersion, STAT_MAJOR_VERSION, STAT_MINOR_VERSION, STAT_REVISION_VERSION);
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Finished comparing version, sending ackowledgement with return value %d\n", intRet);
                if (stream->send(PROT_CHECK_VERSION_RESP, "%d %d %d %d %d", majorVersion, minorVersion, revisionVersion, intRet, 0) == -1)
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

            case PROT_SEND_BROADCAST_STREAM:
                broadcastStream_ = stream;
                intRet = 0;
                ackTag = PROT_SEND_BROADCAST_STREAM_RESP;
                break;

#ifdef STAT_FGFS
            case PROT_SEND_FGFS_STREAM:
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
                msrf = new MRNetSymbolReaderFactory(Walker::getSymbolReader(), network_, stream);
                Walker::setSymbolReader(msrf);
                intRet = 0;
                ackTag = PROT_FILE_REQ_RESP;
                break;
#endif /* FGFS */

#ifdef DYSECTAPI
            case PROT_LOAD_SESSION_LIB:
                char *libraryPath, *dysectBuf, **dysectArgv;
                int dysectBufSize, dysectArgc, dysectBufOffset, i;

                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received request to load session library\n");
                if (packet->unpack("%s %d %ac", &libraryPath, &dysectArgc, &dysectBuf, &dysectBufSize) == -1)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "unpack(PROT_LOAD_SESSION_LIB) failed\n");

                    if (sendAck(stream, PROT_LOAD_SESSION_LIB_RESP, intRet) != STAT_OK)
                    {
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_LOAD_SESSION_LIB_RESP) failed\n");
                        return STAT_MRNET_ERROR;
                    }

                    return STAT_MRNET_ERROR;
                }

                dysectBufOffset = 0;
                dysectArgv = (char **)malloc(dysectArgc);
                if (dysectArgv == NULL)
                {
                    printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to allocate %d elements for dysectArgv", strerror(errno), dysectArgc);
                    return STAT_ALLOCATE_ERROR;
                }
                for (i = 0; i < dysectArgc; i++)
                {
                    dysectArgv[i] = strdup(dysectBuf + dysectBufOffset);
                    if (dysectArgv[i] == NULL)
                    {
                        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to strdup %s for dysectArgv[%d]", strerror(errno), dysectBuf + dysectBufOffset, i);
                        return STAT_ALLOCATE_ERROR;
                    }
                    dysectBufOffset += strlen(dysectBuf + dysectBufOffset) + 1;
                }

                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Library to load: %s, with %d args\n", libraryPath, dysectArgv);
                dysectBE_ = new DysectAPI::BE(libraryPath, this, dysectArgc, dysectArgv);
                DysectAPI::ProcessMgr::init(procSet_);
                if(dysectBE_->isLoaded())
                    intRet = 0;
                if (sendAck(stream, PROT_LOAD_SESSION_LIB_RESP, intRet) != STAT_OK)
                {
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "send(PROT_LOAD_SESSION_LIB_RESP) failed\n");
                    return STAT_MRNET_ERROR;
                }

                break;
#endif

            case PROT_EXIT:
                printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Shutting down\n");

#ifdef DYSECTAPI
                if(dysectBE_)
                {
                    dysectBE_->disableTimers();
                }
#endif
                break;

            default:
#ifdef DYSECTAPI
                /* DysectAPI tags encode additional details */
                if(DysectAPI::isDysectTag(tag))
                {
                    if(dysectBE_)
                    {
                        if(dysectBE_->relayPacket(&packet, tag, stream) == DysectAPI::OK)
                        {
                            /* Packet dealt with by DysectAPI */
                        }
                        break;
                    }
                    else
                        printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Dysect library not loaded yet\n");
                }
                else
                    printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Not Dysect packet\n");
#endif

                /* Unknown tag */
                printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "Unknown Tag %d, first = %d, last = %d\n", tag, PROT_ATTACH_APPLICATION, PROT_SEND_NODE_IN_EDGE_RESP);
                break;
        } /* switch(tag) */

        if (ackTag == PROT_SEND_NODE_IN_EDGE_RESP || ackTag == PROT_SEND_LAST_TRACE_RESP || ackTag == PROT_SEND_TRACES_RESP)
        {
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Sending serialized contents to FE with tag %d\n", ackTag);
            bitVectorLength = statBitVectorLength(proctabSize_);
#ifdef MRNET40
            if (stream->send(ackTag, "%Ac %d %d %ud", byteArray, byteArrayLen, bitVectorLength, myRank_, sampleType_) == -1)
#else
            if (stream->send(ackTag, "%ac %d %d %ud", byteArray, byteArrayLen, bitVectorLength, myRank_, sampleType_) == -1)
#endif
            {
                printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::send(%d) failure\n", ackTag);
                return STAT_MRNET_ERROR;
            }
            if (stream->flush() == -1)
            {
                printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "stream::flush() failure\n");
                return STAT_MRNET_ERROR;
            }
        }
        else if (ackTag != -1)
        {
            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "sending ackowledgement with tag %d (first = %d) and return value %d\n", ackTag, FirstApplicationTag, intRet);
            if (sendAck(broadcastStream_, ackTag, intRet) != STAT_OK)
            {
                printMsg(STAT_MRNET_ERROR, __FILE__, __LINE__, "sendAck(%d) failed\n", ackTag);
                return STAT_MRNET_ERROR;
            }
        }

    } while (tag != PROT_EXIT);

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Received message to exit\n");

    /* Free allocated byte arrays */
    if (byteArray != NULL)
    {
        free(byteArray);
        byteArray = NULL;
    }

#ifdef STAT_FGFS
    delete msrf;
#endif

    return STAT_OK;
}


StatError_t STAT_BackEnd::attach()
{
    int i;
    Walker *proc = NULL;
    map<int, Walker *>::iterator processMapIter;
#if defined(GROUP_OPS)
    vector<ProcessSet::AttachInfo> aInfo;
    ProcessSet::AttachInfo pAttach;
    Process::ptr pcProc;
  #ifdef BGL
    #ifdef SW_VERSION_8_1_3
    /* TODO: version 8.1.3 (or 8.2) doesn't exist with this feature yet */
    BGQData::setStartupTimeout(600);
    #endif
  #endif
#endif

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Attaching to all application processes\n");


#if defined(GROUP_OPS)
  #ifndef OMP_STACKWALKER
    ThreadTracking::setDefaultTrackThreads(false);
  #endif
  #ifdef SW_VERSION_8_1_0
    LWPTracking::setDefaultTrackLWPs(false);
  #endif
    if (doGroupOps_)
    {
        aInfo.reserve(proctabSize_);
        for (i = 0; i < proctabSize_; i++)
        {
//            printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Group attach includes process %s, pid %d, MPI rank %d\n", proctab_[i].pd.executable_name, proctab_[i].pd.pid, proctab_[i].mpirank);
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
#if defined(GROUP_OPS)
    map<int, string> mpiRankToErrMsg;
#endif

    for (i = 0; i < proctabSize_; i++)
    {
//        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Attaching to process %s, pid %d, MPI rank %d\n", proctab_[i].pd.executable_name, proctab_[i].pd.pid, proctab_[i].mpirank);

#if defined(GROUP_OPS)
        if (doGroupOps_)
        {
            pcProc = aInfo[i].proc;
            if (!pcProc)
            {
                stringstream ss;
                ss << "[PC Attach Error - 0x" << std::hex << aInfo[i].error_ret << std::dec << "]";
                mpiRankToErrMsg.insert(make_pair(proctab_[i].mpirank, ss.str()));
            }
            else
            {
                proc = Walker::newWalker(pcProc);
                if (!proc)
                {
                    stringstream ss;
                    ss << "[SW Attach Error - 0x" << std::hex << Stackwalker::getLastError() << std::dec << "]";
                    mpiRankToErrMsg.insert(make_pair(proctab_[i].mpirank, ss.str()));
                }
                else
                {
                    pcProc->setData(proc); /* Up ptr for mapping Process::ptr -> Walker */
                    walkerSet_->insert(proc);
#ifdef DYSECTAPI
                    mpiRankToProcessMap_.insert(pair<int, Process::ptr>(proctab_[i].mpirank, pcProc));
#endif
                }
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
    {
        procsToRanks_.insert(make_pair(processMapIter->second, i));

#if defined(GROUP_OPS)
        int mpirank = processMapIter->first;
        map<int, string>::iterator j = mpiRankToErrMsg.find(mpirank);
        if (j != mpiRankToErrMsg.end())
        {
            string errMsg = j->second;
            exitedProcesses_[errMsg].insert(i);
        }
#endif
    }


    isRunning_ = false;

    return STAT_OK;
}


StatError_t STAT_BackEnd::pause()
{
    map<int, Walker *>::iterator processMapIter;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Pausing all application processes\n");

#if defined(GROUP_OPS)
    if (doGroupOps_)
  #ifdef DYSECTAPI
    {
        if(DysectAPI::ProcessMgr::isActive())
        {
            DysectAPI::ProcessMgr::setWasRunning();
            ProcessSet::ptr allProcs = DysectAPI::ProcessMgr::getAllProcs();
            if(allProcs && !allProcs->empty())
                allProcs->stopProcs();
        }
        else
            procSet_->stopProcs();
    }
  #else
        procSet_->stopProcs();
  #endif
    else
#endif
    {
        for (processMapIter = processMap_.begin(); processMapIter != processMap_.end(); processMapIter++)
           pauseProcess(processMapIter->second);
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
  #ifdef DYSECTAPI
    {
        if(DysectAPI::ProcessMgr::isActive())
        {
            ProcessSet::ptr stopped = DysectAPI::ProcessMgr::getWasRunning();
            if(stopped && !stopped->empty())
                stopped->continueProcs();

        }
        else {
            if(procSet_->anyThreadStopped()) {
                procSet_->continueProcs();
            }
        }
    }
  #else
        procSet_->continueProcs();
  #endif
    else
#endif
    {
        for (processMapIter = processMap_.begin(); processMapIter != processMap_.end(); processMapIter++)
            resumeProcess(processMapIter->second);
    }
    isRunning_ = true;

    return STAT_OK;
}


StatError_t STAT_BackEnd::pauseProcess(Walker *walker)
{
    bool boolRet;
    ProcDebug *pDebug = NULL;

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


StatError_t STAT_BackEnd::resumeProcess(Walker *walker)
{
    bool boolRet;
    ProcDebug *pDebug = NULL;

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
    LibraryState *libState = NULL;
    LibAddrPair lib;
    Symtab *symtab = NULL;
    vector<LineNoTuple *> lines;

    //printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Getting source line info for address %lx\n", addr);

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
        printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Failed to get library at address 0x%lx\n", addr);
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
    //printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "address %lx resolved to %s:%d\n", addr, outBuf, *lineNum);

    return STAT_OK;
}


StatError_t STAT_BackEnd::getVariable(const Frame &frame, char *variableName, char *outBuf, unsigned int outBufSize)
{
    int intRet;
#if defined(PROTOTYPE_TO) || defined(PROTOTYPE_PY)
    int i;
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
        {
            boolRet = func->getLocalVariables(vars);
            if (boolRet == false)
            {
                printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to get local variables for frame %s\n", frameName.c_str());
                return STAT_STACKWALKER_ERROR;
            }
        }
        else if (i == 1)
        {
            boolRet = func->getParams(vars);
            if (boolRet == false)
            {
                printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to get parameters for frame %s\n", frameName.c_str());
                return STAT_STACKWALKER_ERROR;
            }
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
    intRet = -1;
    memcpy(outBuf, &intRet, sizeof(int));
#endif
    return STAT_OK;
}


StatError_t STAT_BackEnd::sampleStackTraces(unsigned int nTraces, unsigned int traceFrequency, unsigned int nRetries, unsigned int retryFrequency, char *variableSpecification)
{
    int j;
    unsigned int i;
    bool wasRunning, isLastTrace;
    graphlib_error_t graphlibError;
    StatError_t statError;
    map<int, Walker *>::iterator processMapIter;
    map<int, StatBitVectorEdge_t *>::iterator nodeInEdgeMapIter;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Preparing to sample %d traces each %d us with %d retries every %d us with variables %s and type %d\n", nTraces, traceFrequency, nRetries, retryFrequency, variableSpecification, sampleType_);

    wasRunning = isRunning_;
    if (sampleType_ & STAT_SAMPLE_CLEAR_ON_SAMPLE)
        clear3dNodesAndEdges();

    if (strcmp(variableSpecification, "NULL") != 0)
    {
        statError = parseVariableSpecification(variableSpecification);
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Failed to parse variable specification\n");
            return statError;
        }
    }

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

        /* Continue the process if necessary */
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


std::string STAT_BackEnd::getFrameName(map<string, string> &nodeAttrs, const Frame &frame, int depth)
{
    int lineNum, i, value, pyLineNo;
    bool boolRet;
    char buf[BUFSIZE], fileName[BUFSIZE], *pyFun = NULL, *pySource = NULL;
    string name;
    Address addr;
    StatError_t statError;

    if (!(sampleType_ & STAT_SAMPLE_MODULE_OFFSET))
    {
        boolRet = frame.getName(name);
        if (boolRet == false)
            name = "[unknown]";
        else if (name == "")
            name = "[empty]";
        nodeAttrs["function"] = name;
    }

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
                nodeAttrs["function"] = pyFun;
                nodeAttrs["source"] = pySource;
                snprintf(buf, BUFSIZE, ":%d", pyLineNo);
                nodeAttrs["line"] = buf;
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
        if (pyFun != NULL)
            free(pyFun);
        if (pySource != NULL)
            free(pySource);
    }

    /* Gather PC value or line number info if requested */
    if (sampleType_ & STAT_SAMPLE_PC)
    {
#ifdef SW_VERSION_8_0_0
        if (frame.isTopFrame() == true)
            snprintf(buf, BUFSIZE, "@0x%lx", frame.getRA());
        else
#endif
            snprintf(buf, BUFSIZE, "@0x%lx", frame.getRA() - 1);
        name += buf;
        nodeAttrs["pc"] = buf;
    }
    else if (sampleType_ & STAT_SAMPLE_MODULE_OFFSET)
    {
        Dyninst::Offset offset;
        string modName;
        void *symtab = NULL;

        boolRet = frame.getLibOffset(modName, offset, symtab);
        if (boolRet == false)
            return "error";
#ifdef SW_VERSION_8_0_0
        if (frame.isTopFrame() == false)
            offset = offset - 1;
#endif
        name = modName;
        snprintf(buf, BUFSIZE, "+0x%lx", offset);
        name += buf;
        nodeAttrs["module"] = modName;
        nodeAttrs["offset"] = buf;
    }
    else if (sampleType_ & STAT_SAMPLE_LINE)
    {
#ifdef SW_VERSION_8_0_0
  #ifdef OMP_STACKWALKER
        if (frame.isTopFrame() == true and !(sampleType_ & STAT_SAMPLE_OPENMP))
  #else
        if (frame.isTopFrame() == true)
  #endif
            addr = frame.getRA();
        else
#endif
            addr = frame.getRA() - 1;
        statError = getSourceLine(frame.getWalker(), addr, fileName, &lineNum);
        if (statError != STAT_OK)
        {
            printMsg(statError, __FILE__, __LINE__, "Failed to get source line information for %s\n", name.c_str());
            return name;
        }
        name += "@";
        name += fileName;
        nodeAttrs["source"] = fileName;
        if (lineNum != 0)
            snprintf(buf, BUFSIZE, ":%d", lineNum);
        else
            snprintf(buf, BUFSIZE, ":?");
        name += buf;
        nodeAttrs["line"] = buf;
        if (extractVariables_ != NULL)
        {
            for (i = 0; i < nVariables_; i++)
            {
                if (strcmp(fileName, extractVariables_[i].fileName) == 0 &&
                    lineNum == extractVariables_[i].lineNum &&
                    depth == extractVariables_[i].depth)
                {
                    statError = getVariable(frame, extractVariables_[i].variableName, buf, BUFSIZE);
                    if (statError != STAT_OK)
                    {
                       printMsg(statError, __FILE__, __LINE__, "Failed to get variable information for $%s in %s\n", extractVariables_[i].variableName, name.c_str());
                       continue;
                    }
                    value = *((int *)buf);
                    snprintf(buf, BUFSIZE, "$%s=%d", extractVariables_[i].variableName, value);
                    name += buf;
                    nodeAttrs["vars"] = nodeAttrs["vars"] + buf;
                }
            }
        } /* if (extractVariables_ != NULL) */
    } /* else if (sampleType_ & STAT_SAMPLE_LINE) */

    return name;
}


StatError_t STAT_BackEnd::getStackTrace(Walker *proc, int rank, unsigned int nRetries, unsigned int retryFrequency)
{
    int nodeId, prevId, i, j, partialTraceScore;
    string::size_type delimPos;
    bool boolRet, isFirstPythonFrame;
    string name, path;
    vector<Frame> currentStackWalk, bestStackWalk;
    vector<string> trace;
    map<string, string>::iterator nodeAttrsIter;
    map<string, map<string, string> > nameToNodeAttrs;
    StatBitVectorEdge_t *edge = NULL;
    vector<Dyninst::THR_ID> threads;
    ProcDebug *pDebug = NULL;
#ifdef GRAPHLIB_3_0
    static int threadCountWarning = 0;
#endif

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
            statFreeEdge(edge);
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
                    printMsg(STAT_WARNING, __FILE__, __LINE__, "Get threads failed... using null thread id instead to just check main thread\n");
                    threads.push_back(NULL_THR_ID);
                }
                else
                    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Gathering trace from %d threads\n", threads.size());
            }
        }
    }

#ifdef OMP_STACKWALKER
    OpenMPStackWalker *ompWalker;
    if (sampleType_ & STAT_SAMPLE_OPENMP)
        ompWalker = new OpenMPStackWalker(proc);
#endif

    /* Loop over the threads and get the traces */
    for (j = 0; j < threads.size(); j++)
    {
#ifdef GRAPHLIB_3_0
        if (find(threadIds_.begin(), threadIds_.end(), threads[j]) == threadIds_.end())
        {
            threadIds_.push_back(threads[j]);
            if (threadCountWarning == 0 and threadIds_.size() >= threadBvLength_)
            {
                printMsg(STAT_WARNING, __FILE__, __LINE__, "Number of threads exceeded %d limit\n", threadBvLength_);
                threadCountWarning++;
            }
        }
#endif
        trace.clear();
        prevId = 0;
        path = "";
        partialTraceScore = 0;

        if (proc == NULL)
            trace.push_back("[task_exited]"); /* We're not attached so return a trace denoting the task has exited */
        else
        {
            for (i = 0; i <= nRetries; i++)
            {
                currentStackWalk.clear();
#ifdef OMP_STACKWALKER
                boolRet = false;
                if (sampleType_ & STAT_SAMPLE_OPENMP)
                    boolRet = ompWalker->walkOpenMPStack(currentStackWalk, threads[j]);
                if (!boolRet)
                {
                    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Failed to get OpenMP translated stack walk, reverting to raw stack walk\n");
                    boolRet = proc->walkStack(currentStackWalk, threads[j]);
                }
#else
                boolRet = proc->walkStack(currentStackWalk, threads[j]);
#endif
                if (boolRet == false && currentStackWalk.size() < 1)
                {
                    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "RETRY failed walk, on attempt %d of %d, thread %d id %d with StackWalker error %s\n", i, nRetries, j, threads[j], Stackwalker::getLastErrorMsg());
                    if (i < nRetries)
                    {
                        if (isRunning_ == false)
                            resumeProcess(proc);
                        usleep(retryFrequency);
                        if (isRunning_ == false)
                            pauseProcess(proc);
                    }
                    continue;
                }

                /* Get the name of the top frame */
                boolRet = currentStackWalk[currentStackWalk.size() - 1].getName(name);
                if (boolRet == false)
                {
                    if (partialTraceScore < 1)
                    {
                        partialTraceScore = 1;
                        bestStackWalk = currentStackWalk;
                    }
                    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "RETRY: top of stack = unknown frame on attempt %d of %d\n", i, nRetries);
                    if (i < nRetries)
                    {
                        if (isRunning_ == false)
                            resumeProcess(proc);
                        usleep(retryFrequency);
                        if (isRunning_ == false)
                            pauseProcess(proc);
                    }
                    continue;
                }

                /* Make sure the top frame contains the string "start" (for main thread only) */
                if (name.find("start", 0) == string::npos && j == 0)
                {
                    if (partialTraceScore < 2)
                    {
                        partialTraceScore = 2;
                        bestStackWalk = currentStackWalk;
                    }
                    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "RETRY: top of stack '%s' not start on attempt %d of %d\n", name.c_str(), i, nRetries);
                    if (i < nRetries)
                    {
                        if (isRunning_ == false)
                            resumeProcess(proc);
                        usleep(retryFrequency);
                        if (isRunning_ == false)
                            pauseProcess(proc);
                    }
                    continue;
                }

                /* The trace looks good so we'll use this one */
                bestStackWalk = currentStackWalk;
                break;
            }

            /* Check to see if we got a complete stack trace */
            if (i > nRetries && partialTraceScore < 1)
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
                    map<string, string> nodeAttrs;
                    name = getFrameName(nodeAttrs, bestStackWalk[i], bestStackWalk.size() - i + 1);
                    if (sampleType_ & STAT_SAMPLE_PYTHON)
                    {
                        if (isPyTrace_ == true && isFirstPythonFrame == true)
                        {
                            /* This is our new root, drop preceding frames */
                            trace.clear();
                            isFirstPythonFrame = false;
                        }

                        if (name.find("PyCFunction_Call") != string::npos)
                        {
                            /* This indicates the end of the Python interpreter */
                            continue;
                        }
                        if (name.find("call_function") != string::npos || name.find("fast_function") != string::npos)
                            continue;
                    }
                    trace.push_back(name);
                    nameToNodeAttrs[name] = nodeAttrs;
                }
            }
        } /* else branch of if (proc == NULL) */

        for (i = 0; i < trace.size(); i++)
        {
            path += trace[i].c_str();
            nodeId = statStringHash(path.c_str());
            nodes2d_[nodeId] = trace[i];
#ifdef GRAPHLIB_3_0
            map<string, string> nodeAttrs = nameToNodeAttrs[trace[i].c_str()];
            for (nodeAttrsIter = nodeAttrs.begin(); nodeAttrsIter != nodeAttrs.end(); nodeAttrsIter++)
                nodeIdToAttrs_[nodeId][nodeAttrsIter->first] = nodeAttrsIter->second;
#endif
            update2dEdge(prevId, nodeId, edge, threads[j]);
            prevId = nodeId;
        }
        trace.clear();
    } /* for j thread */

#ifdef OMP_STACKWALKER
    if (sampleType_ & STAT_SAMPLE_OPENMP)
        delete ompWalker;
#endif
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
    map<string, string> nodeAttrs;
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
        name = getFrameName(nodeAttrs, *frame);
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
#ifdef GRAPHLIB_3_0
            nodeIdToAttrs_[newChildId]["function"] = name;
#endif
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
            update2dEdge(graphlibNode, newChildId, edge, 0);
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


bool statFrameCmp(const Frame &frame1, const Frame &frame2)
{
    int depth = -1;
    string name1, name2;
    map<string, string> nodeAttrs;

#ifdef PROTOTYPE_TO
    /* TODO: compute the depth of each frame for variable extraction */
#endif
    name1 = gBePtr->getFrameName(nodeAttrs, frame1, depth);
    name2 = gBePtr->getFrameName(nodeAttrs, frame2, depth);

    if (name1.compare(name2) < 0)
        return true;
    return false;
}


StatError_t STAT_BackEnd::getStackTraceFromAll(unsigned int nRetries, unsigned int retryFrequency)
{
    int dummyBranches = 0;
    bool swSuccess, dummyAbort = false;
    string dummyString;
    set<int> ranks;
    StatError_t statError;

#ifdef SW_VERSION_8_1_0
    if (procSet_->getLWPTracking() && sampleType_ & STAT_SAMPLE_THREADS)
        procSet_->getLWPTracking()->refreshLWPs();
#endif

    if (procSet_->anyDetached())
    {
        ProcessSet::ptr detachedSubset = procSet_->getDetachedSubset();
        for (ProcessSet::iterator i = detachedSubset->begin(); i != detachedSubset->end(); i++)
        {
            stringstream ss;
            ss << "[Task Detached]";
            Walker *walker = static_cast<Walker *>((*i)->getData());
            map<Walker *, int>::iterator j = procsToRanks_.find(walker);
            if (j != procsToRanks_.end())
                exitedProcesses_[ss.str()].insert(j->second);
        }
        for (WalkerSet::iterator i = walkerSet_->begin(); i != walkerSet_->end(); )
        {
            ProcDebug *pDebug = dynamic_cast<ProcDebug *>((*i)->getProcessState());
            if (detachedSubset->find(pDebug->getProc()) != detachedSubset->end())
            {
                walkerSet_->erase(i++);
                continue;
            }
            i++;
        }
        procSet_ = procSet_->set_difference(detachedSubset);
    }
    if (procSet_->anyExited())
    {
        ProcessSet::ptr exitedSubset = procSet_->getExitedSubset();
        for (ProcessSet::iterator i = exitedSubset->begin(); i != exitedSubset->end(); i++)
        {
            stringstream ss;
            ss << "[Task Exited with " << (*i)->getExitCode() << "]";
            Walker *walker = static_cast<Walker *>((*i)->getData());
            map<Walker *, int>::iterator j = procsToRanks_.find(walker);
            if (j != procsToRanks_.end())
                exitedProcesses_[ss.str()].insert(j->second);
        }
        for (WalkerSet::iterator i = walkerSet_->begin(); i != walkerSet_->end(); )
        {
            ProcDebug *pDebug = dynamic_cast<ProcDebug *>((*i)->getProcessState());
            if (exitedSubset->find(pDebug->getProc()) != exitedSubset->end())
            {
                walkerSet_->erase(i++);
                continue;
            }
            i++;
        }
        procSet_ = procSet_->set_difference(exitedSubset);
    }
    if (procSet_->anyCrashed())
    {
        ProcessSet::ptr crashedSubset = procSet_->getCrashedSubset();
        for (ProcessSet::iterator i = crashedSubset->begin(); i != crashedSubset->end(); i++)
        {
            stringstream ss;
            ss << "[Task Crashed with Signal " << (*i)->getCrashSignal() << "]";
            Walker *walker = static_cast<Walker *>((*i)->getData());
            map<Walker *, int>::iterator j = procsToRanks_.find(walker);
            if (j != procsToRanks_.end())
                exitedProcesses_[ss.str()].insert(j->second);
        }
        for (WalkerSet::iterator i = walkerSet_->begin(); i != walkerSet_->end(); )
        {
            ProcDebug *pDebug = dynamic_cast<ProcDebug *>((*i)->getProcessState());
            if (crashedSubset->find(pDebug->getProc()) != crashedSubset->end())
            {
                walkerSet_->erase(i++);
                continue;
            }
            i++;
        }
        procSet_ = procSet_->set_difference(crashedSubset);
    }
    if (procSet_->anyTerminated())
    {
        ProcessSet::ptr terminatedSubset = procSet_->getTerminatedSubset();
        for (ProcessSet::iterator i = terminatedSubset->begin(); i != terminatedSubset->end(); i++)
        {
            stringstream ss;
            ss << "[Task Terminated]";
            Walker *walker = static_cast<Walker *>((*i)->getData());
            map<Walker *, int>::iterator j = procsToRanks_.find(walker);
            if (j != procsToRanks_.end())
                exitedProcesses_[ss.str()].insert(j->second);
        }
        for (WalkerSet::iterator i = walkerSet_->begin(); i != walkerSet_->end(); )
        {
            ProcDebug *pDebug = dynamic_cast<ProcDebug *>((*i)->getProcessState());
            if (terminatedSubset->find(pDebug->getProc()) != terminatedSubset->end())
            {
                walkerSet_->erase(i++);
                continue;
            }
            i++;
        }
        procSet_ = procSet_->set_difference(terminatedSubset);
    }

#if defined(PROTOTYPE_TO) || defined(PROTOTYPE_PY)
    CallTree tree(statFrameCmp);
#else
    CallTree tree(frame_lib_offset_cmp);
#endif

#ifdef SW_VERSION_8_1_0
    swSuccess = walkerSet_->walkStacks(tree, !(sampleType_ & STAT_SAMPLE_THREADS));
#else
    swSuccess = walkerSet_->walkStacks(tree);
#endif
    if (!swSuccess)
    {
        ProcessSet::ptr errset = procSet_->getErrorSubset();
        if (!errset->empty())
        {
            /* Add any process that hit an error during SWing to the exited list */
            for (ProcessSet::iterator i = errset->begin(); i != errset->end(); i++)
            {
                Process::ptr err_proc = *i;
                stringstream ss;
                ss << "[Stackwalk Error - 0x" << std::hex << err_proc->getLastError() << "]";
                string err_string = ss.str();

                Walker *walker = static_cast<Walker *>(err_proc->getData());
                map<Walker *, int>::iterator j = procsToRanks_.find(walker);
                if (j != procsToRanks_.end())
                    exitedProcesses_[err_string].insert(j->second);
            }
            /* Remove any error'd process from the walkerSet */
            for (WalkerSet::iterator i = walkerSet_->begin(); i != walkerSet_->end(); )
            {
                ProcDebug *pDebug = dynamic_cast<ProcDebug *>((*i)->getProcessState());
                if (errset->find(pDebug->getProc()) != errset->end())
                {
                    walkerSet_->erase(i++);
                    continue;
                }
                i++;
            }
            procSet_ = procSet_->set_difference(errset);
        }
    }

    isPyTrace_ = false;
    statError = addFrameToGraph(&tree, 0, tree.getHead(), dummyString, NULL, ranks, dummyAbort, dummyBranches);
    if (statError != STAT_OK)
    {
        printMsg(statError, __FILE__, __LINE__, "Failed to getStackTraceFromAll\n");
        return statError;
    }

    /* Add error and exited processes to tree */
    for (map<string, set<int> >::iterator i = exitedProcesses_.begin(); i != exitedProcesses_.end(); i++)
    {
        string msg = i->first;
        set<int> &pids = i->second;

        int newChildId = statStringHash(msg.c_str());
        nodes2d_[newChildId] = msg;
#ifdef GRAPHLIB_3_0
        nodeIdToAttrs_[newChildId]["function"] = msg;
#endif

        StatBitVectorEdge_t *edge = initializeBitVectorEdge(proctabSize_);
        if (edge == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to initialize edge\n");
            return STAT_ALLOCATE_ERROR;
        }
        for (set<int>::iterator i = pids.begin(); i != pids.end(); i++)
        {
            int rank = *i;
            edge->bitVector[rank / STAT_BITVECTOR_BITS] |= STAT_GRAPH_BIT(rank % STAT_BITVECTOR_BITS);
        }

        update2dEdge(0, newChildId, edge, 0);
        statFreeEdge(edge);
    }

    return STAT_OK;
}
#endif /* if defined(GROUP_OPS) */


MPIR_PROCDESC_EXT *STAT_BackEnd::getProctab()
{
    return proctab_;
}

int STAT_BackEnd::getProctabSize()
{
    return proctabSize_;
}

int statRelativeRankToAbsoluteRank(int rank)
{
    int absoluteRank = 0;
    if (gBePtr->getProctab() != NULL &&  rank >= 0 && rank <= gBePtr->getProctabSize())
        absoluteRank = gBePtr->getProctab()[rank].mpirank;
    return absoluteRank;
}

int statDummyRelativeRankToAbsoluteRank(int rank)
{
    return rank;
}

StatError_t STAT_BackEnd::detach(unsigned int *stopArray, int stopArrayLen)
{
    int i;
    bool leaveStopped;
    map<int, Walker *>::iterator processMapIter;
    ProcDebug *pDebug = NULL;

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Detaching from application processes, leaving %d processes stopped\n", stopArrayLen);

#if defined(GROUP_OPS)
    if (doGroupOps_ && stopArrayLen == 0)
  #ifdef DYSECTAPI
    {
        if(DysectAPI::ProcessMgr::isActive())
            DysectAPI::ProcessMgr::detachAll();
        else
            procSet_->temporaryDetach();
    }
  #else
        procSet_->detach();
  #endif
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
        }
        processMap_.clear();
        processMapNonNull_ = 0;
    }
    return STAT_OK;
}


StatError_t STAT_BackEnd::terminate()
{
    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Terminating application processes\n");

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
    int parent, daemon, nChildren, nParents, child, currentParentPort, currentParentRank;
    char currentParent[STATBE_MAX_HN_LEN], *ptr = (char *)buf;
    StatLeafInfoArray_t *leafInfoArray = (StatLeafInfoArray_t *)data;

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
    daemon = -1;
    for (parent = 0; parent < nParents; parent++)
    {
        /* Get the parent host name, port, rank and child count */
        strncpy(currentParent, ptr, STATBE_MAX_HN_LEN);
        ptr += strlen(currentParent) + 1;
        memcpy(&currentParentPort, ptr, sizeof(int));
        ptr += sizeof(int);
        memcpy(&currentParentRank, ptr, sizeof(int));
        ptr += sizeof(int);
        memcpy(&nChildren, ptr, sizeof(int));
        ptr += sizeof(int);

        /* Iterate over this parent's children */
        for (child = 0; child < nChildren; child++)
        {
            daemon++;
            if (daemon >= leafInfoArray->size)
            {
                fprintf(stderr, "Failed to unpack STAT info from the front end.  Expecting only %d daemons, but received %d\n", leafInfoArray->size, daemon);
                return -1;
            }

            /* Fill in the parent information */
            strncpy((leafInfoArray->leaves)[daemon].parentHostName, currentParent, STATBE_MAX_HN_LEN);
            (leafInfoArray->leaves)[daemon].parentRank = currentParentRank;
            (leafInfoArray->leaves)[daemon].parentPort = currentParentPort;

            /* Get the daemon host name */
            strncpy((leafInfoArray->leaves)[daemon].hostName, ptr, STATBE_MAX_HN_LEN);
            ptr += strlen((leafInfoArray->leaves)[daemon].hostName) + 1;

            /* Get the daemon rank */
            memcpy(&((leafInfoArray->leaves)[daemon].rank), ptr, sizeof(int));
            ptr += sizeof(int);
        }
    } /* for parent */

    return 0;
}


StatError_t STAT_BackEnd::startLog(unsigned int logType, char *logOutDir, int mrnetOutputLevel, bool withPid)
{
    char fileName[BUFSIZE];

    logType_ = logType;
    snprintf(logOutDir_, BUFSIZE, "%s", logOutDir);

    if (logType_ & STAT_LOG_BE || logType_ & STAT_LOG_SW)
    {
        if (withPid == true)
        {
            pid_t myPid = getpid();
            snprintf(fileName, BUFSIZE, "%s/%s.STATD.%d.log", logOutDir, localHostName_, myPid);
        }
        else
            snprintf(fileName, BUFSIZE, "%s/%s.STATD.log", logOutDir, localHostName_);
        gStatOutFp = fopen(fileName, "w");
        if (gStatOutFp == NULL)
        {
            printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: fopen failed for %s\n", strerror(errno), fileName);
            return STAT_FILE_ERROR;
        }
#ifdef MRNET40
        if (logType_ & STAT_LOG_MRN)
            mrn_printf_init(gStatOutFp);
#endif
        set_OutputLevel(mrnetOutputLevel);
    }

    if (logType_ & STAT_LOG_SWERR || logType_ & STAT_LOG_SW)
    {
        if (logType_ & STAT_LOG_SW)
            swDebugFile_ = gStatOutFp;
        else
        {
            swLogBuffer_.reset();
            swDebugFile_ = swLogBuffer_.handle();
            if (swDebugFile_ == NULL)
            {
                printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s: Failed to allocate circular buffer for SW logging\n", strerror(errno));
                return STAT_ALLOCATE_ERROR;
            }
        }
        Dyninst::Stackwalker::setDebug(true);
        Dyninst::Stackwalker::setDebugChannel(swDebugFile_);
#ifdef SW_VERSION_8_0_0
        Dyninst::ProcControlAPI::setDebug(true);
        Dyninst::ProcControlAPI::setDebugChannel(swDebugFile_);
#endif
	registerSignalHandlers(true);
    }

    return STAT_OK;
}


void STAT_BackEnd::printMsg(StatError_t statError, const char *sourceFile, int sourceLine, const char *fmt, ...)
{
    va_list args;
    char timeString[BUFSIZE], msg[BUFSIZE];
    const char *timeFormat = "%b %d %T";
    time_t currentTime;
    struct tm *localTime = NULL;

    if (statError == STAT_LOG_MESSAGE && !(logType_ & STAT_LOG_BE))
        return;

    currentTime = time(NULL);
    localTime = localtime(&currentTime);
    if (localTime == NULL)
        snprintf(timeString, BUFSIZE, "NULL");
    else
        strftime(timeString, BUFSIZE, timeFormat, localTime);

    if (statError != STAT_LOG_MESSAGE)
    {
        fprintf(errOutFp_, "<%s> <%s:%d> ", timeString, sourceFile, sourceLine);
        if (statError == STAT_WARNING)
            fprintf(errOutFp_, "%s: STAT_WARNING: ", localHostName_);
        else
        {
            fprintf(errOutFp_, "%s: STAT returned error type ", localHostName_);
            statPrintErrorType(statError, errOutFp_);
            fprintf(errOutFp_, ": ");
        }
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
            if (sourceLine != -1 && sourceFile != NULL)
                mrn_printf(sourceFile, sourceLine, "", gStatOutFp, "%s", msg);
            else
                fprintf(gStatOutFp, "%s", msg);
            fflush(gStatOutFp);
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
    int statOutFD;
    char fileName[BUFSIZE];

    if (logType_ & STAT_LOG_SWERR)
    {
        if (swDebugFile_ != NULL)
        {
            if (gStatOutFp == NULL)
            {
                snprintf(fileName, BUFSIZE, "%s/%s.STATD.log", logOutDir_, localHostName_);
                gStatOutFp = fopen(fileName, "w");
                if (gStatOutFp == NULL)
                {
                    printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: fopen failed for %s\n", strerror(errno), fileName);
                    return;
                }
#ifdef MRNET40
                if (logType_ & STAT_LOG_MRN)
                    mrn_printf_init(gStatOutFp);
#endif
            }
            fflush(gStatOutFp);
            statOutFD = fileno(gStatOutFp);
            fflush(swDebugFile_);
            swLogBuffer_.flushBufferTo(statOutFD);
            swLogBuffer_.reset();
        }
    }
}


vector<Field *> *STAT_BackEnd::getComponents(Type *type)
{
    typeTypedef *tt = NULL;
    typeStruct *ts = NULL;
    vector<Field *> *components = NULL;

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
    int i, found = 0, fLastIVal = -1, address, lineNo, firstLineNo;
    long length, pAddr;
    unsigned long long baseAddr, pyCodeObjectBaseAddr, pyUnicodeObjectAddr;
    bool boolRet;
    char buffer[BUFSIZE], varName[BUFSIZE], exePath[BUFSIZE];
    static map<string, StatPythonOffsets_t *> pythonOffsetsMap;
    StatPythonOffsets_t *pythonOffsets = NULL;
    Symtab *symtab = NULL;
    Type *type = NULL;
    vector<Field *> *components = NULL;
    Field *field = NULL;
#ifdef SW_VERSION_8_0_0
    LibraryPool::iterator libsIter;
#endif
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
    if (pythonOffsetsMap.find(exePath) == pythonOffsetsMap.end())
    {
        pythonOffsets = (StatPythonOffsets_t *)malloc(sizeof(StatPythonOffsets_t));
        if (pythonOffsets == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to allocate memory for pythonOffsets\n");
            return STAT_ALLOCATE_ERROR;
        }
        *pythonOffsets = (StatPythonOffsets_t){-1, -1, -1, -1, -1, -1, -1, -1, -1, false, false};
        pythonOffsetsMap[exePath] = pythonOffsets;
    }
    else
        pythonOffsets = pythonOffsetsMap[exePath];

    /* Find all necessary variable field offsets */
    if (pythonOffsets->coNameOffset == -1 || pythonOffsets->coFileNameOffset == -1 || pythonOffsets->obSizeOffset == -1 || pythonOffsets->obSvalOffset == -1 || pythonOffsets->coFirstLineNoOffset == -1 || pythonOffsets->coLnotabOffset == -1 || pythonOffsets->fLastIOffset == -1)
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
                pythonOffsets->fLastIOffset = field->getOffset() / 8;
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
                pythonOffsets->coFirstLineNoOffset = field->getOffset() / 8;
                found++;
            }
            else if (strcmp(field->getName().c_str(), "co_lnotab") == 0)
            {
                pythonOffsets->coLnotabOffset = field->getOffset() / 8;
                found++;
            }
            else if (strcmp(field->getName().c_str(), "co_filename") == 0)
            {
                pythonOffsets->coFileNameOffset = field->getOffset() / 8;
                found++;
            }
            else if (strcmp(field->getName().c_str(), "co_name") == 0)
            {
                pythonOffsets->coNameOffset = field->getOffset() / 8;
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
            pythonOffsets->isUnicode = true;
            pythonOffsets->isPython3 = true;
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
            if (pythonOffsets->isUnicode == true)
            {
                /* Python 3.2 */
                /* Note Python 3.3 will run through here, but won't find these fields.  We will deal with this later */
                if (strcmp(field->getName().c_str(), "str") == 0)
                {
                    pythonOffsets->obSvalOffset = field->getOffset() / 8;
                    found++;
                }
                else if (strcmp(field->getName().c_str(), "length") == 0)
                {
                    pythonOffsets->obSizeOffset = field->getOffset() / 8;
                    found++;
                }
            }
            else
            {
                /* Python 2.X */
                if (strcmp(field->getName().c_str(), "ob_sval") == 0)
                {
                    pythonOffsets->obSvalOffset = field->getOffset() / 8;
                    found++;
                }
                else if (strcmp(field->getName().c_str(), "ob_size") == 0)
                {
                    pythonOffsets->obSizeOffset = field->getOffset() / 8;
                    found++;
                }
            }
        } /* for i */
    } /* if any static offset field == -1, i.e., has not been set yet */

    if (pythonOffsets->obSvalOffset == -1 and pythonOffsets->obSizeOffset == -1 and pythonOffsets->isUnicode == true)
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
                pythonOffsets->obSizeOffset = field->getOffset() / 8;
                break;
            }
        }
        pythonOffsets->obSvalOffset = type->getSize() + 4; /* string right after object, not exactly sure why + 4 necessary */
        pythonOffsets->isUnicode = false; /* the data is stored as regular C string in this case */
    }

    if (pythonOffsets->isPython3 == true && pythonOffsets->pyVarObjectObSizeOffset == -1 && pythonOffsets->pyBytesObjectObSvalOffset == -1)
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
                pythonOffsets->pyVarObjectObSizeOffset = field->getOffset() / 8;
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
                pythonOffsets->pyBytesObjectObSvalOffset = field->getOffset() / 8;
                break;
            }
        }
    } /* if is Python 3 and static fields not set */

    if (pythonOffsets->coNameOffset == -1 || pythonOffsets->coFileNameOffset == -1 || pythonOffsets->obSizeOffset == -1 || pythonOffsets->obSvalOffset == -1 || pythonOffsets->coFirstLineNoOffset == -1 || pythonOffsets->coLnotabOffset == -1 || pythonOffsets->fLastIOffset == -1)
    {
         printMsg(STAT_STACKWALKER_ERROR, __FILE__, __LINE__, "Failed to find offsets for Python script source and line info %d %d %d %d %d %d %d\n", pythonOffsets->coNameOffset, pythonOffsets->coFileNameOffset, pythonOffsets->obSizeOffset, pythonOffsets->obSvalOffset, pythonOffsets->coFirstLineNoOffset, pythonOffsets->coLnotabOffset, pythonOffsets->fLastIOffset);
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
        pDebug->readMem(buffer, pyCodeObjectBaseAddr + pythonOffsets->fLastIOffset, sizeof(int));
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
    pDebug->readMem(buffer, pyCodeObjectBaseAddr + pythonOffsets->coNameOffset, sizeof(unsigned long long));
    baseAddr = *((unsigned long long *)buffer);
    pDebug->readMem(buffer, baseAddr + pythonOffsets->obSizeOffset, sizeof(unsigned long long));
    length = *((long *)buffer);
    if (length >= BUFSIZE)
        length = BUFSIZE - 1;
    if (pythonOffsets->isUnicode == true)
    {
        pDebug->readMem(buffer, baseAddr + pythonOffsets->obSvalOffset, sizeof(unsigned long long));
        pyUnicodeObjectAddr = *((unsigned long long *)buffer);
        /* We can read every other byte to get the C string representation of the unicode */
        for (i = 0; i < length; i++)
            pDebug->readMem(&buffer[i], pyUnicodeObjectAddr + 2 * i, 1);
        buffer[length] = '\0';
        *pyFun = strdup(buffer);
    }
    else
    {
        pDebug->readMem(buffer, baseAddr + pythonOffsets->obSvalOffset, length);
        buffer[length] = '\0';
        *pyFun = strdup(buffer);
    }
    if (*pyFun == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed on call to strdup(%s) to *pyFun)\n", strerror(errno), buffer);
        return STAT_ALLOCATE_ERROR;
    }

    /* Get the Python source file name */
    pDebug->readMem(buffer, pyCodeObjectBaseAddr + pythonOffsets->coFileNameOffset, sizeof(unsigned long long));
    baseAddr = *((unsigned long long *)buffer);
    pDebug->readMem(buffer, baseAddr + pythonOffsets->obSizeOffset, sizeof(unsigned long long));
    length = *((long *)buffer);
    if (length >= BUFSIZE)
        length = BUFSIZE - 1;
    if (pythonOffsets->isUnicode == true)
    {
        pDebug->readMem(buffer, baseAddr + pythonOffsets->obSvalOffset, sizeof(unsigned long long));
        pyUnicodeObjectAddr = *((unsigned long long *)buffer);
        for (i = 0; i < length; i++)
            pDebug->readMem(&buffer[i], pyUnicodeObjectAddr + 2 * i, 1);
        buffer[length] = '\0';
        *pySource = strdup(buffer);
    }
    else
    {
        pDebug->readMem(buffer, baseAddr + pythonOffsets->obSvalOffset, length);
        buffer[length] = '\0';
        *pySource = strdup(buffer);
    }
    if (*pySource == NULL)
    {
        printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "%s Failed on call to strdup(%s) to *pySource)\n", strerror(errno), buffer);
        return STAT_ALLOCATE_ERROR;
    }

    /* Get the Python source line number */
    if (fLastIVal != -1)
    {
        pDebug->readMem(buffer, pyCodeObjectBaseAddr + pythonOffsets->coFirstLineNoOffset, sizeof(unsigned long long));
        baseAddr = *((unsigned long long *)buffer);
        pDebug->readMem(buffer, baseAddr + pythonOffsets->obSvalOffset, sizeof(int));
        firstLineNo = *((int *)buffer);
        pDebug->readMem(buffer, pyCodeObjectBaseAddr + pythonOffsets->coLnotabOffset, sizeof(unsigned long long));
        baseAddr = *((unsigned long long *)buffer);
        if (pythonOffsets->isPython3 == true)
        {
            pDebug->readMem(buffer, baseAddr + pythonOffsets->pyVarObjectObSizeOffset, sizeof(unsigned long long));
            length = *((long *)buffer);
            pAddr = baseAddr + pythonOffsets->pyBytesObjectObSvalOffset;
        }
        else
        {
            pDebug->readMem(buffer, baseAddr + pythonOffsets->obSizeOffset, sizeof(unsigned long long));
            length = *((long *)buffer);
            pAddr = baseAddr + pythonOffsets->obSvalOffset;
        }
        if (length >= BUFSIZE)
            length = BUFSIZE - 1;
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
    unsigned int bytesWritten;
    char fileName[BUFSIZE], data[BUFSIZE], *ptr = NULL;
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
    for (i = 0, count = -1; i < leafInfoArray.size; i++)
    {
        XPlat::NetUtils::GetHostName(localHostName_, prettyHost);
        XPlat::NetUtils::GetHostName(string(leafInfoArray.leaves[i].hostName), leafPrettyHost);
        if (prettyHost == leafPrettyHost)
        {
            count++;
            if (count >= proctabSize_)
            {
                printMsg(STAT_LMON_ERROR, __FILE__, __LINE__, "The size of the leaf information for this node appears to be larger than the process table size\n");
                return STAT_LMON_ERROR;
            }

            /* Wait for the daemon emulator to create its fifo then write its connection info */
            snprintf(fileName, BUFSIZE, "/tmp/%s.%d.statbench.txt", localHostName_, count);
            while (1)
            {
                intRet = access(fileName, W_OK);
                if (intRet == 0)
                    break;
                usleep(1000);
            }

            fd = open(fileName, O_WRONLY);
            if (fd == -1)
            {
                printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: open() failed for %s\n", strerror(errno), fileName);
                remove(fileName);
                return STAT_FILE_ERROR;
            }

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

            intRet = close(fd);
            if (intRet != 0)
            {
                printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: close() failed for fifo %s, fd %d\n", strerror(errno), fileName, fd);
                remove(fileName);
                return STAT_FILE_ERROR;
            }
        }
    }

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

    if (leafInfoArray.leaves != NULL)
        free(leafInfoArray.leaves);

    return STAT_OK;
}

StatError_t STAT_BackEnd::statBenchConnect()
{
    int i, intRet, fd, bytesRead, inPort, inParentRank, inRank, mpiRank;
    char fileName[BUFSIZE], inHostName[BUFSIZE], data[BUFSIZE], *ptr = NULL, *param[6], parentPort[BUFSIZE], parentRank[BUFSIZE], myRank[BUFSIZE];

    for (i = 0; i < 8192; i++)
    {
        snprintf(fileName, BUFSIZE, "/tmp/%s.%d.statbench.txt", localHostName_, i);
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

    fd = open(fileName, O_RDONLY);
    if (fd == -1)
    {
        printMsg(STAT_FILE_ERROR, __FILE__, __LINE__, "%s: open() failed for fifo %s\n", strerror(errno), fileName);
        remove(fileName);
        return STAT_FILE_ERROR;
    }

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
    if (network_ == NULL)
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

    printMsg(STAT_LOG_MESSAGE, __FILE__, __LINE__, "Creating traces with max depth = %d, num tasks = %d, num traces = %d, function fanout = %d, equivalence classes = %d, sample type = %u\n", maxDepth, nTasks, nTraces, functionFanout, nEqClasses, sampleType_);

    if (init == 0)
    {
        proctabSize_ = nTasks;
        proctab_ = (MPIR_PROCDESC_EXT *)malloc(proctabSize_ * sizeof(MPIR_PROCDESC_EXT));
        if (proctab_ == NULL)
        {
            printMsg(STAT_ALLOCATE_ERROR, __FILE__, __LINE__, "Failed to allocate %d bytes for proctab_\n", proctabSize_);
            return STAT_ALLOCATE_ERROR;
        }
        proctab_[0].mpirank = myRank_ * nTasks;
        for (i = 0; i < proctabSize_; i++)
        {
            proctab_[i].pd.executable_name = NULL;
            proctab_[i].pd.host_name = NULL;
            proctab_[i].mpirank = proctab_[0].mpirank + i;
        }
        init++;
    }

    clear3dNodesAndEdges();

    for (i = 0; i < nTraces; i++)
    {
        clear2dNodesAndEdges();
        for (j = 0; j < nTasks; j++)
        {
            /* We now create the full eq class set at once */
            if (j >= nEqClasses && nEqClasses != -1)
                break;
            statError = statBenchCreateTrace(maxDepth, j, nTasks, functionFanout, nEqClasses, i);
            if (statError != STAT_OK)
            {
                printMsg(statError, __FILE__, __LINE__, "Failed to create trace %d of %d for task %d of %d\n", i + 1, nTraces, j, nTasks);
                return statError;
            }
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
    char frame[BUFSIZE];
    string path;
    StatBitVectorEdge_t *edge = NULL;

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
#ifdef GRAPHLIB_3_0
    nodeIdToAttrs_[nodeId]["function"] = nodes2d_[nodeId];
#endif
    update2dEdge(prevId, nodeId, edge, 0);
    prevId = nodeId;

    /* Create artificial main */
    path += "main";
    nodeId = statStringHash(path.c_str());
    nodes2d_[nodeId] = "main";
#ifdef GRAPHLIB_3_0
    nodeIdToAttrs_[nodeId]["function"] = nodes2d_[nodeId];
#endif
    update2dEdge(prevId, nodeId, edge, 0);
    prevId = nodeId;

    /* Seed the random number generator based on my equivalence class */
    if (nEqClasses == -1) /* no limit */
        srand(task + 999999 * iteration);
    else
        srand(task % nEqClasses +  999999 * (1 + iteration));

    depth = rand() % maxDepth;
    for (i = 0; i < depth; i++)
    {
        snprintf(frame, BUFSIZE, "depth%dfun%d", i, rand() % functionFanout);
        path += frame;
        nodeId = statStringHash(path.c_str());
        nodes2d_[nodeId] = frame;
#ifdef GRAPHLIB_3_0
        nodeIdToAttrs_[nodeId]["function"] = nodes2d_[nodeId];
#endif
        update2dEdge(prevId, nodeId, edge, 0);
        prevId = nodeId;
    }

    statFreeEdge(edge);

    return STAT_OK;
}


